#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "log_service.h"
#include "atcmd_mqtt.h"
#include <lwip_netconf.h>
#include "tcpip.h"
#include <dhcp/dhcps.h>
#include "atcmd_wifi.h"
#if CONFIG_WLAN
#include "wifi_conf.h"
#endif

#if ATCMD_VER == ATVER_2
#include "flash_api.h"
#include "device_lock.h"
#endif

#include "platform_opts.h"
#if defined(CONFIG_PLATFORM_8710C)||defined(CONFIG_PLATFORM_8721D)
#include "platform_opts_bt.h"
#endif

#if ((defined CONFIG_MQTT_EN) && (1 == CONFIG_MQTT_EN))

static MQTT_RESULT_ENUM mqttGetHandleCb(u8 tcpConnId, MQTT_CONTROL_BLOCK **mqttCb, u8 needInit);
static void mqttDelHandleCb(MQTT_CONTROL_BLOCK **mqttCb);
static void mqttSetHandleCb(u8 tcpConnId, MQTT_CONTROL_BLOCK *mqttCb);
static MQTT_RESULT_ENUM mqttInitHandleCb(MQTT_CONTROL_BLOCK **pMqttCb);
static MQTT_RESULT_ENUM mqttInitClientBuf(MQTTClient *client, Network *network, u32 timeout);
static void mqttDelClientBuf(MQTTClient *client);
static void mqttMessageArrived(MessageData* data, void *param);
static MQTT_RESULT_ENUM mqttStringCopy(char **dest, char *src, size_t sz);
static void mqttClientAliveFail(MQTT_CONTROL_BLOCK *mqttCb);
static MQTT_RESULT_ENUM mqttClientDataProc(MQTT_CONTROL_BLOCK *mqttCb, fd_set *read_fds, u8 *needAtOutpput);
static u8 mqttConfigGetIndex(char *cmdString);

MQTT_AT_HANDLE gMqttAtHandle;
MQTTPacket_connectData gMqttDefaultConnData = MQTTPacket_connectData_initializer;

const char *gMqttConfigStr[MQTT_CONFIG_CMD_NUM] = {"version", "keepalive", "session", "timeout", "will", "ssl"};

/****************************************************************
Function            mqttMain
Brief                   The main function of mqtt AT task.
****************************************************************/
void mqttMain(void *param)
{
    s32 resultNo = 0;
    MQTT_CONTROL_BLOCK *mqttCb = (MQTT_CONTROL_BLOCK *)param;
    u8 needAtOutpput = 0;
    fd_set read_fds;
#if MQTT_EXCEPT_FDS
    fd_set except_fds;
#endif
    struct timeval timeout;

    if(NULL == mqttCb)
    {
        mqtt_debug("\r\n[mqttMain] Input invalid param.");
        goto end;
    }

    while(1)
    {
        if(MQTT_TASK_START != mqttCb->taskState)
        {
            mqtt_debug("\r\n[mqttMain] Should stop now");
            break;
        }

        FD_ZERO(&read_fds);
#if MQTT_EXCEPT_FDS
        FD_ZERO(&except_fds);
#endif
        timeout.tv_sec = MQTT_SELECT_TIMEOUT;
        timeout.tv_usec = 0;

        if(0 <= mqttCb->network.my_socket)
        {
            FD_SET(mqttCb->network.my_socket, &read_fds);
#if MQTT_EXCEPT_FDS
            FD_SET(mqttCb->network.my_socket, &except_fds);
            resultNo = FreeRTOS_Select(mqttCb->network.my_socket + 1, &read_fds, NULL, &except_fds, &timeout);
            /* The my_socket may be close, then will try reopen in mqttClientDataProc if STATUS set to MQTT_START */
            if(FD_ISSET(mqttCb->network.my_socket, &except_fds))
            {
                //mqtt_debug("\r\n[mqttMain] except_fds is set");
                mqttCb->client.mqttstatus = MQTT_START;
            }
#else
            resultNo = FreeRTOS_Select(mqttCb->network.my_socket + 1, &read_fds, NULL, NULL, &timeout);
            if(FD_ISSET(mqttCb->network.my_socket, &read_fds))
            {
                /* Do nothing here. The data will be processed in mqttClientDataProc( ). */
            }
#endif
            /* Select timeout. */
            else if(0 == resultNo)
            {
                // mqtt_debug("\r\n[mqttMain] Select timeout");
                /* !FIXME */
                if(mqttCb->client.isconnected)
                {
                    keepalive(&mqttCb->client);
                    /* ping_outstanding++ when keepalive send ping req , ping_outstanding-- if receive ping rsp ; 
                    ping_outstandingis >= 2, it means we miss the ping rsp 2*keepalive intervals, maybe the link is error;
                    server will disconnect if not receive ping req in 1.5*keepalive intervals. */
                    if(2 < mqttCb->client.ping_outstanding)
                    {
                        mqttClientAliveFail(mqttCb);
                    }
                }
            }
        }
        else
        {
            //mqtt_debug("\r\n[mqttMain] ERROR, disconnected");
            /* FIXME! Need do anything? */
        }
        /* Process the received data. */
        resultNo = mqttClientDataProc(mqttCb, &read_fds, &needAtOutpput);
        if(1 == needAtOutpput)
        {
            if(MQTT_OK == resultNo)
            {
                at_printf("ACK\r\n");
            }
            else
            {
                at_printf("ERROR:%d\r\n", resultNo);
            }
            ATCMD_NEWLINE_HASHTAG();
        }
    }

end:
    mqtt_debug("\r\n[mqttMain] Stop mqtt task");
    mqttCb->taskState = MQTT_TASK_NOT_CREATE;
    mqttCb->taskHandle = NULL;
    vTaskDelete(mqttCb->taskHandle);
}

/****************************************************************
Function            fATQO
Brief                   MQTT open, The command is used to open a network for MQTT client.
****************************************************************/
void fATQO(void *arg)
{
    const u8 idIndex = 1, hostIndex = 2, portIndex = 3;
    s32 i = 0, res = 0, keep = 0;
    MQTT_RESULT_ENUM resultNo = MQTT_OK;
    char *argv[MAX_ARGC] = {0};
    MQTT_CONTROL_BLOCK *mqttCb = NULL;
    u8  tcpConnId = MQTT_MAX_CLIENT_NUM;
    char TaskName[6];
    char *hostName;
    u16 portId = MQTT_DEFAULT_PORT;
#if MQTT_OVER_SSL
    const u16 stacksize = 6144;
#else
    const u16 stacksize = 4096;
#endif

    /* Get the parameters of AT command. */
    if(!arg)
    {
        AT_DBG_MSG(AT_FLAG_MQTT, AT_DBG_ERROR,
                                "\r\n[ATQO] Usage : ATQO=<tcpConnId>,<hostname>,<port>");
        resultNo = MQTT_ARGS_ERROR;
        goto end;
    }

    /* parameters include tcpConnId, hostname, port. The port is optional. */
    i = parse_param(arg, argv);
    if(4 < i || 3 > i)
    {
        mqtt_debug("\r\n[ATQO] parameters include tcpConnId, hostname, port.");
        resultNo = MQTT_ARGS_ERROR;
        goto end;
    }

    /* connect ID. */
    res = -1;
    if(NULL != argv[idIndex] && 1 == _strlen(argv[idIndex])
        && '0' <= argv[idIndex][0] && '0' + MQTT_MAX_CLIENT_NUM > argv[idIndex][0])
    {
        res = argv[idIndex][0] - '0';
    }
    if(0 > res || MQTT_MAX_CLIENT_NUM <= res)
    {
        mqtt_debug("\r\n[ATQO] Invalid tcpConnId");
        resultNo = MQTT_CLIENTID_REJECTED_ERROR;
        goto end;
    }
    tcpConnId = (u8)res;
    /* For each connect ID, the task name should be unique. */
    TaskName[0] = 'M';
    TaskName[1] = 'Q';
    TaskName[2] = 'T';
    TaskName[3] = 'T';
    TaskName[4] = '0' + tcpConnId;
    TaskName[5] = '\0';

    if(NULL != gMqttAtHandle.mqttCb[tcpConnId])
    {
        mqtt_debug("\r\n[ATQO] tcpConnId %d already exists", tcpConnId);
        keep = 1;
        resultNo = MQTT_ALREADY_EXIST_ID;
        goto end;
    }

    /* hostName. */
    if(NULL == argv[hostIndex] || MQTT_MAX_HOSTNAME_LEN < _strlen(argv[hostIndex]) || 0 == _strlen(argv[hostIndex]))
    {
        mqtt_debug("\r\n[ATQO] Invalid hostName.");
        resultNo = MQTT_ARGS_ERROR;
        goto end;
    }
    hostName = argv[hostIndex];

    /* Port is optional. */
    if(4 == i && NULL != argv[portIndex])
    {
        res = atoi(argv[portIndex]);
        if(0 >= res || 0xFFFF < res)
        {
            mqtt_debug("\r\n[ATQO] Invalid port.");
            resultNo = MQTT_ARGS_ERROR;
            goto end;
        }
        portId = (u16)res;
    }

    /* Check whether the network is linked. */
    if(RTW_SUCCESS != wifi_is_ready_to_transceive(RTW_STA_INTERFACE))
    {
        mqtt_debug("\r\n[ATQO] The network is not ready.");
        resultNo = MQTT_NETWORK_LINK_ERROR;
        goto end;
    }

    /* Get the specific mqtt control block, and initialise. */
    resultNo = mqttGetHandleCb(tcpConnId, &mqttCb, 1);
    if(MQTT_OK != resultNo)
    {
        goto end;
    }

    /* Update the mqttCb information. */
    mqttCb->tcpConnectId = tcpConnId;
    mqttCb->port = portId;
    resultNo = mqttStringCopy(&mqttCb->host, hostName, _strlen(hostName));
    if(MQTT_OK != resultNo)
    {
        goto end;
    }
#if MQTT_OVER_SSL
    mqttCb->network.use_ssl = mqttCb->useSsl;
#endif

    mqtt_debug("\r\n[ATQO] Open a new connection for %s", mqttCb->host);

#if (!MQTT_CONNECT_LATER)
    /* Try to connect the host. */
    if(NULL != mqttCb->taskHandle)
    {
        vTaskSuspend(mqttCb->taskHandle);
    }
    res = NetworkConnect(&mqttCb->network, mqttCb->host, mqttCb->port);
    if(NULL != mqttCb->taskHandle)
    {
        vTaskResume(mqttCb->taskHandle);
    }
    if(0 != res)
    {
        mqtt_debug("\r\n[ATQO] Can not connect %s", mqttCb->host);
        resultNo = MQTT_AUTH_ERROR;
        goto end;
    }
    mqttCb->networkConnect = 1;
#endif

    /* Create the task. */
    mqttCb->taskState = MQTT_TASK_START;
    if(pdPASS != xTaskCreate(mqttMain,
                            TaskName,
                            stacksize,
                            (void *)mqttCb,
                            ATCMD_MQTT_TASK_PRIORITY,
                            &mqttCb->taskHandle))
    {
        mqtt_debug("\r\n[ATQO] Create task failed.");
        mqttCb->taskState = MQTT_TASK_NOT_CREATE;
        resultNo = MQTT_THREAD_CREATE_ERROR;
        goto end;
    }

end:
    if(MQTT_OK != resultNo)
    {
        /* If the connect id is already created before, do not delete it. */
        if(0 == keep)
        {
            mqttDelHandleCb(&mqttCb);
            mqttSetHandleCb(tcpConnId, mqttCb);
        }
        at_printf("%sERROR:%d\r\n", "+MQTTOPEN:", resultNo);
    }
    else
    {
        at_printf("%sOK\r\n", "+MQTTOPEN:");
    }
    ATCMD_NEWLINE_HASHTAG();
}

/****************************************************************
Function            fATQC
Brief                   MQTT close. The command is used to close a network for MQTT client.
****************************************************************/
void fATQC(void *arg)
{
    const u8 idIndex = 1;
    s32  i = 0, res = 0;
    MQTT_RESULT_ENUM resultNo = MQTT_OK;
    char *argv[MAX_ARGC] = {0};
    MQTT_CONTROL_BLOCK *mqttCb = NULL;
    u8  tcpConnId = MQTT_MAX_CLIENT_NUM;

    /* Get the parameters of AT command. */
    if(!arg)
    {
        AT_DBG_MSG(AT_FLAG_MQTT, AT_DBG_ERROR,
                                "\r\n[ATQC] Usage : ATQC=<tcpConnId>");
        resultNo = MQTT_ARGS_ERROR;
        goto end;
    }

    /* parameters include tcpConnId. */
    i = parse_param(arg, argv);
    if(2 != i)
    {
        mqtt_debug("\r\n[ATQC] Input only connect ID");
        resultNo = MQTT_ARGS_ERROR;
        goto end;
    }

    /* Connect ID. */
    res = -1;
    if(NULL != argv[idIndex] && 1 == _strlen(argv[idIndex])
        && '0' <= argv[idIndex][0] && '0' + MQTT_MAX_CLIENT_NUM > argv[idIndex][0])
    {
        res = argv[idIndex][0] - '0';
    }
    if(0 > res || MQTT_MAX_CLIENT_NUM <= res)
    {
        mqtt_debug("\r\n[ATQC] Invalid tcpConnId");
        resultNo = MQTT_CLIENTID_REJECTED_ERROR;
        goto end;
    }
    tcpConnId = (u8)res;

    mqtt_debug("\r\n[ATQC] Close %d", tcpConnId);
    resultNo = mqttGetHandleCb(tcpConnId, &mqttCb, 0);
    if(MQTT_OK != resultNo)
    {
        resultNo = MQTT_NOT_OPEN_ERROR;
        mqtt_debug("\r\n[ATQC] Failed when getting mqttCb");
        goto end;
    }

        /* Stop the task. */
    if(MQTT_TASK_START == mqttCb->taskState)
    {
        mqttCb->taskState = MQTT_TASK_STOP;
        while(MQTT_TASK_NOT_CREATE != mqttCb->taskState)
        {
            vTaskDelay(20);
        }
    }

    /* Disconnect client. */
    if(mqttCb->client.isconnected)
    {
        mqtt_debug("\r\n[ATQC] Still connected");
        res = MQTTDisconnect(&mqttCb->client);
        if(0 != res)
        {
            mqtt_debug("\r\n[ATQC] Can not disconnect");
            resultNo = MQTT_ALREADY_CONNECTED_ERROR;
            goto end;
        }
        mqttCb->client.isconnected = 0;
    }

    /* Disconnect host. */
    if(mqttCb->networkConnect)
    {
        if(NULL != mqttCb->network.disconnect)
        {
            mqtt_debug("\r\n[ATQC] Disconnect from %s", mqttCb->host);
            mqttCb->network.disconnect(&mqttCb->network);
        }
        mqttCb->networkConnect = 0;
    }

    // NetworkDeinit(&mqttCb->network);
    mqttCb->network.my_socket = -1;
    mqttCb->network.mqttread = NULL;
    mqttCb->network.mqttwrite = NULL;
    mqttCb->network.disconnect = NULL;
#if MQTT_OVER_SSL
    mqttCb->network.use_ssl = 0;
    mqttCb->network.ssl = NULL;
#if CONFIG_USE_MBEDTLS
    mqttCb->network.conf = NULL;
#endif
#endif

end:
    /* Clean the Client Control block. */
    mqttDelHandleCb(&mqttCb);
    mqttSetHandleCb(tcpConnId, mqttCb);

    if(MQTT_OK != resultNo)
    {
        at_printf("%sERROR:%d\r\n", "+MQTTCLOSE:", resultNo);
    }
    else
    {
        at_printf("%sOK\r\n", "+MQTTCLOSE:");
    }
    ATCMD_NEWLINE_HASHTAG();
}

/****************************************************************
Function            fATQN
Brief                   MQTT connect. The command is used to connect a cloud for MQTT client.
****************************************************************/
void fATQN(void *arg)
{
    const u8 idIndex = 1, clientIndex = 2, usernameIndex = 3, passwordIndex = 4;
    s32 i = 0, res = 0;
    MQTT_RESULT_ENUM resultNo = MQTT_OK;
    char *argv[MAX_ARGC] = {0};
    u8  tcpConnId = MQTT_MAX_CLIENT_NUM;
    MQTT_CONTROL_BLOCK *mqttCb = NULL;
    // char *clientId = NULL, *username = NULL, *password = NULL;

    /* Get the parameters of AT command. */
    if(!arg)
    {
        AT_DBG_MSG(AT_FLAG_MQTT, AT_DBG_ERROR,
                                "\r\n[ATQN] Usage : ATQN=<tcpConnId>,<param>,<paramValue> or ATQN=<tcpConnId>,\"send\"");
        resultNo = MQTT_ARGS_ERROR;
        goto end;
    }

    /* tcpconnectID, clientID are mandetory, username, password are optional. */
    i = parse_param(arg, argv);
    if(3 > i || 5 < i)
    {
        mqtt_debug("\r\n[ATQN] Usage : ATQN=<tcpConnId>,<client>[,<username>,<password>]");
        resultNo = MQTT_ARGS_ERROR;
        goto end;
    }

    /* Connect ID. */
    res = -1;
    if(NULL != argv[idIndex] && 1 == _strlen(argv[idIndex])
        && '0' <= argv[idIndex][0] && '0' + MQTT_MAX_CLIENT_NUM > argv[idIndex][0])
    {
        res = argv[idIndex][0] - '0';
    }
    if(0 > res || MQTT_MAX_CLIENT_NUM <= res)
    {
        mqtt_debug("\r\n[ATQN] Invalid tcpConnId");
        resultNo = MQTT_CLIENTID_REJECTED_ERROR;
        goto end;
    }
    tcpConnId = (u8)res;

    resultNo = mqttGetHandleCb(tcpConnId, &mqttCb, 0);
    if(MQTT_OK != resultNo)
    {
        resultNo = MQTT_NOT_OPEN_ERROR;
        goto end;
    }

    /* Client ID. */
    if(NULL == argv[clientIndex] || 0 == _strlen(argv[clientIndex]) || MQTT_MAX_CLIENT_ID_LEN < _strlen(argv[clientIndex]))
    {
        mqtt_debug("\r\n[ATQN] Invalid client ID");
        resultNo = MQTT_CLIENTID_REJECTED_ERROR;
        goto end;
    }
    resultNo = mqttStringCopy(&mqttCb->clientId, argv[clientIndex], _strlen(argv[clientIndex]));
    if(MQTT_OK != resultNo)
    {
        goto end;
    }

    /* username. */
    if(i > usernameIndex)
    {
        if(NULL == argv[usernameIndex] || 0 == _strlen(argv[usernameIndex]) || MQTT_MAX_USERNAME_LEN < _strlen(argv[usernameIndex]))
        {
            mqtt_debug("\r\n[ATQN] Invalid username");
            resultNo = MQTT_CLIENTID_REJECTED_ERROR;
            goto end;
        }
        resultNo = mqttStringCopy(&mqttCb->userName, argv[usernameIndex], _strlen(argv[usernameIndex]));
        if(MQTT_OK != resultNo)
        {
            goto end;
        }
    }

    /* password. */
    if(i > passwordIndex)
    {
        if(NULL == argv[passwordIndex] || 0 == _strlen(argv[passwordIndex]) || MQTT_MAX_PASSWORD_LEN < _strlen(argv[passwordIndex]))
        {
            mqtt_debug("\r\n[ATQN] Invalid password");
            resultNo = MQTT_CLIENTID_REJECTED_ERROR;
            goto end;
        }
        resultNo = mqttStringCopy(&mqttCb->password, argv[passwordIndex], _strlen(argv[passwordIndex]));
        if(MQTT_OK != resultNo)
        {
            goto end;
        }
    }

    /* send. */
#if (!MQTT_CONNECT_LATER)
    if(0 > mqttCb->network.my_socket && NULL != mqttCb->host)
#endif
    {
        mqtt_debug("\r\n[ATQN] Connect to host %s", mqttCb->host);
        if(NULL != mqttCb->taskHandle)
        {
            vTaskSuspend(mqttCb->taskHandle);
        }
        res = NetworkConnect(&mqttCb->network, mqttCb->host, mqttCb->port);
        if(NULL != mqttCb->taskHandle)
        {
            vTaskResume(mqttCb->taskHandle);
        }
        if(0 != res)
        {
            mqtt_debug("\r\n[ATQN] Can not connect to host %s", mqttCb->host);
            mqttCb->client.mqttstatus = MQTT_START;
            resultNo = MQTT_CONNECTION_ERROR;
            goto end;
        }
        mqttCb->networkConnect = 1;
        mqttCb->client.isconnected = 0;
    }
    /* Check status. */
    if(mqttCb->client.isconnected)
    {
        mqtt_debug("\r\n[ATQN] Already connected");
        resultNo = MQTT_NOT_ATTACH_ERROR;
        goto end;
    }
    else
    {
        //mqtt_debug("\r\n[ATQN] Reset, then execute");
        /* Reset when reconnect. */
        mqttCb->client.ping_outstanding = 0;
        mqttCb->client.next_packetid = 1;
    }
    if(NULL == mqttCb->clientId)
    {
        mqtt_debug("\r\n[ATQN] The clientid has not been set");
        resultNo = MQTT_COMMON_ERROR;
        goto end;
    }
    mqttCb->connectData.clientID.cstring = mqttCb->clientId;
    /* The connect may be anonymous, ie. without username and password. */
    if(NULL != mqttCb->userName)
    {
        mqttCb->connectData.username.cstring = mqttCb->userName;
    }
    if(NULL != mqttCb->password)
    {
        mqttCb->connectData.password.cstring = mqttCb->password;
    }
    /* Set timer. */
    TimerInit(&mqttCb->client.cmd_timer);
    TimerCountdownMS(&mqttCb->client.cmd_timer, mqttCb->client.command_timeout_ms);
    res = MQTTConnect(&mqttCb->client, &mqttCb->connectData);
    if(0 != res)
    {
        mqtt_debug("\r\n[ATQN] Connect ERROR");
        resultNo = MQTT_CONNECTION_ERROR;
        goto end;
    }
    else
    {
        /* AT output when receiving CONNACK. */
        mqtt_debug("\r\n[ATQN] Sent connection request, waiting for ACK");
        mqttCb->client.mqttstatus = MQTT_CONNECT;
        mqttCb->initialConnect = 1;
    }

end:
    if(MQTT_OK != resultNo)
    {
        at_printf("%sERROR:%d\r\n", "+MQTTCONN:", resultNo);
    }
    else
    {
        at_printf("%sOK\r\n", "+MQTTCONN:");
    }
    ATCMD_NEWLINE_HASHTAG();
}

/****************************************************************
Function            fATQD
Brief                   MQTT disconnect. The command is used to disconnect a cloud for MQTT client.
****************************************************************/
void fATQD(void *arg)
{
    const u8 idIndex = 1;
    s32 i = 0, res = 0;
    MQTT_RESULT_ENUM resultNo = MQTT_OK;
    char *argv[MAX_ARGC] = {0};
    u8  tcpConnId = MQTT_MAX_CLIENT_NUM;
    MQTT_CONTROL_BLOCK *mqttCb = NULL;

    /* Get the parameters of AT command. */
    if(!arg)
    {
        AT_DBG_MSG(AT_FLAG_MQTT, AT_DBG_ERROR,
                                "\r\n[ATQD] Usage : ATQD=<tcpConnId>");
        resultNo = MQTT_ARGS_ERROR;
        goto end;
    }

    i = parse_param(arg, argv);
    if(2 != i)
    {
        mqtt_debug("\r\n[ATQD] Usage : ATQD=<tcpConnId>");
        resultNo = MQTT_ARGS_ERROR;
        goto end;
    }

    /* Connect ID. */
    res = -1;
    if(NULL != argv[idIndex] && 1 == _strlen(argv[idIndex])
        && '0' <= argv[idIndex][0] && '0' + MQTT_MAX_CLIENT_NUM > argv[idIndex][0])
    {
        res = argv[idIndex][0] - '0';
    }
    if(0 > res || MQTT_MAX_CLIENT_NUM <= res)
    {
        mqtt_debug("\r\n[ATQD] Invalid connect ID");
        resultNo = MQTT_ARGS_ERROR;
        goto end;
    }
    tcpConnId = (u8)res;

    mqtt_debug("\r\n[ATQD] Disconnect connId %d", tcpConnId);
    resultNo = mqttGetHandleCb(tcpConnId, &mqttCb, 0);
    if(MQTT_OK != resultNo)
    {
        resultNo = MQTT_NOT_OPEN_ERROR;
        goto end;
    }

    if(!mqttCb->client.isconnected)
    {
        mqtt_debug("\r\n[ATQD] ALREADY disconnected");
        resultNo = MQTT_NOT_CONNECTED_ERROR;
        goto end;
    }

    res = MQTTDisconnect(&mqttCb->client);
    if(0 != res)
    {
        mqtt_debug("\r\n[ATQD] Disconnect ERROR");
        resultNo = MQTT_COMMON_ERROR;
        goto end;
    }

end:
    /* Delete the stored clientid, username, password. Set the flag back. */
    if(NULL != mqttCb)
    {
        if(NULL != mqttCb->clientId)
        {
            free(mqttCb->clientId);
            mqttCb->clientId = NULL;
        }
        if(NULL != mqttCb->userName)
        {
            free(mqttCb->userName);
            mqttCb->userName = NULL;
        }
        if(NULL != mqttCb->password)
        {
            free(mqttCb->password);
            mqttCb->password = NULL;
        }
        mqttCb->offline = 0;
        mqttCb->initialConnect = 0;
    }

    if(MQTT_OK != resultNo)
    {
        at_printf("%sERROR:%d\r\n", "+MQTTDISCONN:", resultNo);
    }
    else
    {
        at_printf("%sOK\r\n", "+MQTTDISCONN:");
    }
    ATCMD_NEWLINE_HASHTAG();
}

/****************************************************************
Function            fATQS
Brief                   MQTT subscribe. The command is used to subscribe mqtt issue.
****************************************************************/
void fATQS(void *arg)
{
    const u8 idIndex = 1, topicIndex = 2, qosIndex = 3;
    s32 i = 0, found = -1, res = 0;
    MQTT_RESULT_ENUM resultNo = MQTT_OK;
    char *argv[MAX_ARGC] = {0};
    u8  tcpConnId = MQTT_MAX_CLIENT_NUM;
    u8  qos = MQTT_DEFAULT_QOS;
    u8  j = 0;
    MQTT_CONTROL_BLOCK *mqttCb = NULL;

    /* Get the parameters of AT command. */
    if(!arg)
    {
        AT_DBG_MSG(AT_FLAG_MQTT, AT_DBG_ERROR,
                                "\r\n[ATQS] Usage : ATQS=<tcpconnectid>,<topic>,<qos>");
        resultNo = MQTT_ARGS_ERROR;
        goto end;
    }

    i = parse_param(arg, argv);
    if(3 > i || 4 < i)
    {
        mqtt_debug("\r\n[ATQS] Usage : ATQS=<tcpconnectid>,<msgid>,<topic>,<qos>");
        resultNo = MQTT_ARGS_ERROR;
        goto end;
    }

    /* Connect ID. */
    res = -1;
    if(NULL != argv[idIndex] && 1 == _strlen(argv[idIndex])
        && '0' <= argv[idIndex][0] && '0' + MQTT_MAX_CLIENT_NUM > argv[idIndex][0])
    {
        res = argv[idIndex][0] - '0';
    }
    if(0 > res || MQTT_MAX_CLIENT_ID_LEN <= res)
    {
        mqtt_debug("\r\n[ATQS] Invalid connect ID");
        resultNo = MQTT_ARGS_ERROR;
        goto end;
    }
    tcpConnId = (u8)res;

    resultNo = mqttGetHandleCb(tcpConnId, &mqttCb, 0);
    if(MQTT_OK != resultNo)
    {
        resultNo = MQTT_NOT_OPEN_ERROR;
        goto end;
    }

    /* topic. */
    if(NULL == argv[topicIndex] || 0 == _strlen(argv[topicIndex]) || MQTT_MAX_TOPIC_LEN < _strlen(argv[topicIndex]))
    {
        mqtt_debug("\r\n[ATQS] Invalid topic");
        resultNo = MQTT_ARGS_ERROR;
        goto end;
    }
    for(j = 0; MAX_MESSAGE_HANDLERS > j; j++)
    {
        if(NULL == mqttCb->topic[j])
        {
            if(-1 == found)
            {
                found = j;
            }
        }
        else if(0 == _strcmp(mqttCb->topic[j], argv[topicIndex]))
        {
        	found = -1;
        	break;
        }
    }
    if(-1 == found)
    {
        mqtt_debug("\r\n[ATQS] Can not match.");
        resultNo = MQTT_SUBSCRIPTION_ERROR;
        goto end;
    }
    j = (u8)found;
    resultNo = mqttStringCopy(&mqttCb->topic[j], argv[topicIndex], _strlen(argv[topicIndex]));
    if(MQTT_OK != resultNo)
    {
        goto end;
    }

    /* QoS. This is optional, if not including here, it should be QOS2. */
    if(4 == i)
    {
        res = (NULL != argv[qosIndex]) ? atoi(argv[qosIndex]) : -1;
        if(QOS0 > res || QOS2 < res)
        {
            mqtt_debug("\r\n[ATQS] Invalid QoS");
            resultNo = MQTT_ARGS_ERROR;
            goto end;
        }
        qos = (u8)res;
    }

    /* Check status. */
    if(1 != mqttCb->client.isconnected || 0 > mqttCb->network.my_socket)
    {
        mqtt_debug("\r\n[ATQS] Not connected now (%d, %d)",
                            mqttCb->client.isconnected, mqttCb->network.my_socket);
        resultNo = MQTT_NOT_CONNECTED_ERROR;
        goto end;
    }
#if 0
    if(MQTT_RUNNING == mqttCb->client.mqttstatus)
    {
        mqtt_debug("\r\n[ATQS] Already subscribed");
        resultNo = MQTT_ALREADY_SUBSCRIBED_ERROR;
        goto end;
    }
#endif

    mqtt_debug("\r\n[ATQS] Subscribe topic %s", mqttCb->topic[j]);

    TimerInit(&mqttCb->client.cmd_timer);
    TimerCountdownMS(&mqttCb->client.cmd_timer, mqttCb->client.command_timeout_ms);
    res = MQTTSubscribe(&mqttCb->client, mqttCb->topic[j], qos, mqttMessageArrived);
    if(0 != res)
    {
        mqtt_debug("\r\n[ATQS] Subscribe ERROR");
        resultNo = MQTT_SUBSCRIPTION_ERROR;
        goto end;
    }
    mqttCb->client.mqttstatus = MQTT_SUBTOPIC;

end:
    if(MQTT_OK != resultNo)
    {
        if(-1 != found && NULL != mqttCb->topic[j])
        {
            free(mqttCb->topic[j]);
            mqttCb->topic[j] = NULL;
        }
        at_printf("%sERROR:%d\r\n", "+MQTTSUB:", resultNo);
    }
    else
    {
        at_printf("%sOK\r\n", "+MQTTSUB:");
    }
    ATCMD_NEWLINE_HASHTAG();
}

/****************************************************************
Function            fATQU
Brief                   MQTT unsubscribe. The command is used to unsubscribe mqtt issue.
****************************************************************/
void fATQU(void *arg)
{
    const u8 idIndex = 1, topicIndex = 2;
    s32 i = 0, res = 0;
    MQTT_RESULT_ENUM resultNo = MQTT_OK;
    char *argv[MAX_ARGC] = {0};
    u8 j = 0, matched = 0, tcpConnId = MQTT_MAX_CLIENT_NUM;
    MQTT_CONTROL_BLOCK *mqttCb = NULL;

    /* Get the parameters of AT command. */
    if(!arg)
    {
        AT_DBG_MSG(AT_FLAG_MQTT, AT_DBG_ERROR,
                                "\r\n[ATQU] Usage : ATQU=<tcpconnectid>,<topic>");
        resultNo = MQTT_ARGS_ERROR;
        goto end;
    }

    i = parse_param(arg, argv);
    if(3 != i)
    {
        mqtt_debug("\r\n[ATQU] Usage : ATQU=<tcpconnectid>,<topic>");
        resultNo = MQTT_ARGS_ERROR;
        goto end;
    }

    /* Connect ID. */
    res = -1;
    if(NULL != argv[idIndex] && 1 == _strlen(argv[idIndex])
        && '0' <= argv[idIndex][0] && '0' + MQTT_MAX_CLIENT_NUM > argv[idIndex][0])
    {
        res = argv[idIndex][0] - '0';
    }
    if(0 > res || MQTT_MAX_CLIENT_ID_LEN <= res)
    {
        mqtt_debug("\r\n[ATQU] Invalid connect ID");
        resultNo = MQTT_ARGS_ERROR;
        goto end;
    }
    tcpConnId = (u8)res;

    resultNo = mqttGetHandleCb(tcpConnId, &mqttCb, 0);
    if(MQTT_OK != resultNo)
    {
        resultNo = MQTT_NOT_OPEN_ERROR;
        goto end;
    }

    /* topic. */
    if(NULL == argv[topicIndex] || 0 == _strlen(argv[topicIndex]) || MQTT_MAX_TOPIC_LEN < _strlen(argv[topicIndex]))
    {
        mqtt_debug("\r\n[ATQU] Invalid topic");
        resultNo = MQTT_ARGS_ERROR;
        goto end;
    }
    for(j = 0; MAX_MESSAGE_HANDLERS > j; j++)
    {
        if(NULL != mqttCb->topic[j] && 0 == _strcmp(mqttCb->topic[j], argv[topicIndex]))
        {
            matched = 1;
            break;
        }
    }
    if(0 == matched)
    {
        mqtt_debug("\r\n[ATQU] Invalid topic");
        resultNo = MQTT_ARGS_ERROR;
        goto end;
    }

    /* Check status. */
    if(1 != mqttCb->client.isconnected || 0 > mqttCb->network.my_socket)
    {
        mqtt_debug("\r\n[ATQU] Invalid status (%d, %d)",
                            mqttCb->client.isconnected, mqttCb->network.my_socket);
        resultNo = MQTT_UNSUBSCRIPTION_ERROR;
        goto end;
    }

    mqtt_debug("\r\n[ATQU] Unsubscribe topic %s", mqttCb->topic[j]);

    TimerInit(&mqttCb->client.cmd_timer);
    TimerCountdownMS(&mqttCb->client.cmd_timer, mqttCb->client.command_timeout_ms);
    res = MQTTUnsubscribe(&mqttCb->client, mqttCb->topic[j]);
    if(0 != res)
    {
        mqtt_debug("\r\n[ATQU] Unsubscribe ERROR");
        resultNo = MQTT_UNSUBSCRIPTION_ERROR;
        goto end;
    }

end:
    if(1 == matched && NULL != mqttCb->topic[j])
    {
        free(mqttCb->topic[j]);
        mqttCb->topic[j] = NULL;
        mqttCb->client.messageHandlers[j].topicFilter = 0;
        mqttCb->client.messageHandlers[j].fp = NULL;
    }
    if(MQTT_OK != resultNo)
    {
        at_printf("%sERROR:%d\r\n", "+MQTTUNSUB:", resultNo);
    }
    else
    {
        at_printf("%sOK\r\n", "+MQTTUNSUB:");
    }
    ATCMD_NEWLINE_HASHTAG();
}

/****************************************************************
Function            fATQP
Brief                   MQTT publish. The command is used to publish mqtt issue.
****************************************************************/
void fATQP(void *arg)
{
    const u8 idIndex = 1, msgidIndex = 2, qosIndex = 3, retainIndex = 4, topicIndex = 5, msgIndex = 6;
    s32 i = 0, res = 0;
    MQTT_RESULT_ENUM resultNo = MQTT_OK;
    char *argv[MAX_ARGC] = {0};
    u8 tcpConnId = MQTT_MAX_CLIENT_NUM;
    u16 msgId = 0;
    MQTT_CONTROL_BLOCK *mqttCb = NULL;

    /* Get the parameters of AT command. */
    if(!arg)
    {
        AT_DBG_MSG(AT_FLAG_MQTT, AT_DBG_ERROR,
                "\r\n[ATQP] Usage : ATQP=<tcpconnectid>,<msgid>,<qos>,<retain>,<topic>,<msg>");
        resultNo = MQTT_ARGS_ERROR;
        goto end;
    }

    i = parse_param_advance(arg, argv, msgIndex);
    if(7 != i)
    {
        mqtt_debug("\r\n[ATQP] Usage : ATQP=<tcpconnectid>,<msgid>,<qos>,<retain>,<topic>,<msg>");
        resultNo = MQTT_ARGS_ERROR;
        goto end;
    }

    /* Connect ID. */
    res = -1;
    if(NULL != argv[idIndex] && 1 == _strlen(argv[idIndex])
        && '0' <= argv[idIndex][0] && '0' + MQTT_MAX_CLIENT_NUM > argv[idIndex][0])
    {
        res = argv[idIndex][0] - '0';
    }
    if(0 > res || MQTT_MAX_CLIENT_ID_LEN <= res)
    {
        mqtt_debug("\r\n[ATQP] Invalid connect ID");
        resultNo = MQTT_ARGS_ERROR;
        goto end;
    }
    tcpConnId = (u8)res;

    resultNo = mqttGetHandleCb(tcpConnId, &mqttCb, 0);
    if(MQTT_OK != resultNo)
    {
        resultNo = MQTT_NOT_OPEN_ERROR;
        goto end;
    }

    /* msg ID. */
    res = (NULL != argv[msgidIndex]) ? atoi(argv[msgidIndex]) : -1;
    if(0 >= res || 0xFFFF <= res)
    {
        mqtt_debug("\r\n[ATQP] Invalid msg ID");
        resultNo = MQTT_ARGS_ERROR;
        goto end;
    }
    msgId = (u16)res;

    /* QoS. */
    res = (NULL != argv[qosIndex]) ? atoi(argv[qosIndex]) : -1;
    if(QOS0 > res || QOS2 < res)
    {
        mqtt_debug("\r\n[ATQP] Invalid QoS");
        resultNo = MQTT_ARGS_ERROR;
        goto end;
    }
    mqttCb->pubData.qos = (u8)res;

    /* retain. */
    res = (NULL != argv[retainIndex]) ? atoi(argv[retainIndex]) : -1;
    if(0 > res || 1 < res)
    {
        mqtt_debug("\r\n[ATQP] Invalid retain");
        resultNo = MQTT_ARGS_ERROR;
        goto end;
    }
    mqttCb->pubData.retain = (u8)res;

    /* topic. */
    if(NULL != argv[topicIndex] && 0 != _strlen(argv[topicIndex]) && MQTT_MAX_TOPIC_LEN >= _strlen(argv[topicIndex]))
    {
        resultNo = mqttStringCopy(&mqttCb->pubData.topic, argv[topicIndex], _strlen(argv[topicIndex]));
        if(MQTT_OK != resultNo)
        {
            goto end;
        }
    }

    /* msg. */
    if(NULL != argv[msgIndex] && 0 != _strlen(argv[msgIndex]) && MQTT_MAX_MSG_LEN >= _strlen(argv[msgIndex]))
    {
        resultNo = mqttStringCopy(&mqttCb->pubData.msg, argv[msgIndex], _strlen(argv[msgIndex]));
        if(MQTT_OK != resultNo)
        {
            goto end;
        }
    }

    /* send publish. */
    if(NULL == mqttCb->pubData.msg || NULL == mqttCb->pubData.topic)
    {
        mqtt_debug("\r\n[ATQP] Not prepared");
        resultNo = MQTT_COMMON_ERROR;
        goto end;
    }
    if(1 != mqttCb->client.isconnected)
    {
        mqtt_debug("\r\n[ATQP] Invalid status (%d)", mqttCb->client.isconnected);
        resultNo = MQTT_NOT_CONNECTED_ERROR;
        goto end;
    }
    MQTTMessage mqttMsg = {.qos = mqttCb->pubData.qos,
                                                .retained = mqttCb->pubData.retain,
                                                .dup = 0,
                                                .id = msgId,
                                                .payload = mqttCb->pubData.msg,
                                                .payloadlen = _strlen(mqttCb->pubData.msg)};
    res = MQTTPublish(&mqttCb->client, mqttCb->pubData.topic, &mqttMsg);
    if(0 != res)
    {
        mqtt_debug("\r\n[ATQP] Publish ERROR");
        resultNo = MQTT_PUBLISH_ERROR;
        goto end;
    }

end:
    /* After sent, clean the msg and topic. */
    if(NULL != mqttCb)
    {
        if(NULL != mqttCb->pubData.topic)
        {
            free(mqttCb->pubData.topic);
            mqttCb->pubData.topic = NULL;
        }
        if(NULL != mqttCb->pubData.msg)
        {
            free(mqttCb->pubData.msg);
            mqttCb->pubData.msg = NULL;
        }
    }
    if(MQTT_OK != resultNo)
    {
        at_printf("%sERROR:%d\r\n", "+MQTTPUB:", resultNo);
    }
    else
    {
        at_printf("%sOK\r\n", "+MQTTPUB:");
    }
    ATCMD_NEWLINE_HASHTAG();
}

/****************************************************************
Function            fATQF
Brief                   MQTT configure. The command is used to configure mqtt parameters.
****************************************************************/
void fATQF(void *arg)
{
    const u8 idIndex = 1, paramIndex = 2, valueIndex = 3;
    u8  argc = 0;
    s32 res = 0;
    MQTT_RESULT_ENUM resultNo = MQTT_OK;
    char *argv[MAX_ARGC] = {0};
    u8  tcpConnId = MQTT_MAX_CLIENT_NUM, cmdIdx = MQTT_CONFIG_CMD_NUM;
    MQTT_CONTROL_BLOCK *mqttCb = NULL;

    /* Get the parameters of AT command. */
    if(!arg)
    {
        AT_DBG_MSG(AT_FLAG_MQTT, AT_DBG_ERROR,
                                "\r\n[ATQF] Usage : ATQF=? or ATQF=<parameter>,<value>");
        resultNo = MQTT_ARGS_ERROR;
        goto end;
    }

    argc = parse_param_advance(arg, argv, valueIndex + 4);
    if(3 > argc)
    {
        mqtt_debug("\r\n[ATQF] Input wrong parameter");
        resultNo = MQTT_ARGS_ERROR;
        goto end;
    }

    /* Connect ID. */
    res = -1;
    if(NULL != argv[idIndex] && 1 == _strlen(argv[idIndex])
        && '0' <= argv[idIndex][0] && '0' + MQTT_MAX_CLIENT_NUM > argv[idIndex][0])
    {
        res = argv[idIndex][0] - '0';
    }
    if(MQTT_MAX_CLIENT_NUM <= res || 0 > res)
    {
        mqtt_debug("\r\n[ATQF] Invalid tcpConnId");
        resultNo = MQTT_ARGS_ERROR;
        goto end;
    }
    tcpConnId = (u8)res;

    resultNo = mqttGetHandleCb(tcpConnId, &mqttCb, 0);
    if(MQTT_OK != resultNo)
    {
        resultNo = MQTT_NOT_OPEN_ERROR;
        mqtt_debug("\r\n[ATQF] The connect ID does not exist");
        goto end;
    }

    /* Get command. */
    if('?' == argv[paramIndex][0])
    {
        at_printf("%sMQTTVersion %d\r\n", "+MQTTCFG:", mqttCb->connectData.MQTTVersion);
        at_printf("%skeepAliveInterval %d\r\n", "+MQTTCFG:", mqttCb->connectData.keepAliveInterval);
        at_printf("%scleansession %d\r\n", "+MQTTCFG:", mqttCb->connectData.cleansession);
        at_printf("%scommand_timeout_ms %d (ms)\r\n", "+MQTTCFG:", mqttCb->client.command_timeout_ms);
        at_printf("%swillFlag %d\r\n", "+MQTTCFG:", mqttCb->connectData.willFlag);
        if(mqttCb->connectData.willFlag)
        {
            MQTTPacket_willOptions *will = &mqttCb->connectData.will;
            at_printf("%sqos %d, retained %d, topicName %s, message %s\r\n",
                                "+MQTTCFG:", will->qos, will->retained, will->topicName.cstring, will->message.cstring);
        }
#if MQTT_OVER_SSL
        at_printf("%suseSsl %d\r\n", "+MQTTCFG:", mqttCb->useSsl);
#else
        at_printf("%suseSsl not support\r\n", "+MQTTCFG:");
#endif
    }

    /* Set command. */
    else
    {
        cmdIdx = mqttConfigGetIndex(argv[paramIndex]);
        switch(cmdIdx)
        {
            /* "version" */
            case MQTT_CONFIG_VERSION:
                if(4 > argc)
                {
                    mqtt_debug("\r\n[ATQF] No version number");
                    resultNo = MQTT_ARGS_ERROR;
                    goto end;
                }
                res = (NULL != argv[valueIndex]) ? atoi(argv[valueIndex]) : -1;
                if(3 != res && 4 != res)
                {
                    mqtt_debug("\r\n[ATQF] Invalid version number");
                    resultNo = MQTT_ARGS_ERROR;
                    goto end;
                }
                mqttCb->connectData.MQTTVersion = (u8)res;
                break;

            /* "keepalive" */
            case MQTT_CONFIG_KEEPALIVE:
                if(4 > argc)
                {
                    mqtt_debug("\r\n[ATQF] No keepalive");
                    resultNo = MQTT_ARGS_ERROR;
                    goto end;
                }
                res = (NULL != argv[valueIndex]) ? atoi(argv[valueIndex]) : -1;
                if(0 >= res || 3600 < res)
                {
                    mqtt_debug("\r\n[ATQF] Invalid keepalive");
                    resultNo = MQTT_ARGS_ERROR;
                    goto end;
                }
                mqttCb->connectData.keepAliveInterval = (u16)res;
                break;

            /* "session" */
            case MQTT_CONFIG_SESSION:
                if(4 > argc)
                {
                    mqtt_debug("\r\n[ATQF] No session");
                    resultNo = MQTT_ARGS_ERROR;
                    goto end;
                }
                res = (NULL != argv[valueIndex]) ? atoi(argv[valueIndex]) : -1;
                if(0 != res && 1 != res)
                {
                    mqtt_debug("\r\n[ATQF] Invalid session");
                    resultNo = MQTT_ARGS_ERROR;
                    goto end;
                }
                mqttCb->connectData.cleansession = (u8)res;
                break;

            /* "timeout" */
            case MQTT_CONFIG_TIMEOUT:
                if(4 > argc)
                {
                    mqtt_debug("\r\n[ATQF] No timeout");
                    resultNo = MQTT_ARGS_ERROR;
                    goto end;
                }
                res = (NULL != argv[valueIndex]) ? atoi(argv[valueIndex]) : -1;
                if(10000 > res || 60000 < res)
                {
                    mqtt_debug("\r\n[ATQF] Invalid timeout");
                    resultNo = MQTT_ARGS_ERROR;
                    goto end;
                }
                mqttCb->client.command_timeout_ms = (u32)res;
                break;

            /* "will" */
            case MQTT_CONFIG_WILL:
                if(4 > argc)
                {
                    mqtt_debug("\r\n[ATQF] No will value");
                    resultNo = MQTT_ARGS_ERROR;
                    goto end;
                }
                res = (NULL != argv[valueIndex]) ? atoi(argv[valueIndex]) : -1;
                if(0 == res)
                {
                    mqttCb->connectData.willFlag = 0;
                }
                else if(1 == res)
                {
                    if(8 > argc)
                    {
                        mqtt_debug("\r\n[ATQF] No will value");
                        resultNo = MQTT_ARGS_ERROR;
                        goto end;
                    }
                    mqttCb->connectData.willFlag = 1;
                    mqttCb->connectData.will.qos = (s8)atoi(argv[valueIndex + 1]);
                    mqttCb->connectData.will.retained = (u8)atoi(argv[valueIndex + 2]);
                    resultNo = mqttStringCopy(&mqttCb->connectData.will.topicName.cstring, argv[valueIndex + 3], _strlen(argv[valueIndex + 3]));
                    if(MQTT_OK != resultNo)
                    {
                        goto end;
                    }
                    resultNo = mqttStringCopy(&mqttCb->connectData.will.message.cstring, argv[valueIndex + 4], _strlen(argv[valueIndex + 4]));
                    if(MQTT_OK != resultNo)
                    {
                        goto end;
                    }
                }
                else
                {
                    mqtt_debug("\r\n[ATQF] Invalid will value");
                    resultNo = MQTT_ARGS_ERROR;
                    goto end;
                }
                break;

            /* "ssl" */
            case MQTT_CONFIG_SSL:
#if MQTT_OVER_SSL
                if(4 > argc)
                {
                    mqtt_debug("\r\n[ATQF] No ssl");
                    resultNo = MQTT_ARGS_ERROR;
                    goto end;
                }
                res = (NULL != argv[valueIndex]) ? atoi(argv[valueIndex]) : -1;
                if(0 != res && 1 != res)
                {
                    mqtt_debug("\r\n[ATQF] Invalid ssl");
                    resultNo = MQTT_ARGS_ERROR;
                    goto end;
                }
                mqttCb->useSsl = (u8)res;
#else
                mqtt_debug("\r\n[ATQF] Not support");
                resultNo = MQTT_ARGS_ERROR;
                goto end;
#endif
                break;

            default:
                mqtt_debug("\r\n[ATQF] Input error parameters");
                resultNo = MQTT_ARGS_ERROR;
                goto end;
                break;
        }
    }

end:
    if(MQTT_OK == resultNo)
    {
        at_printf("%sOK\r\n", "+MQTTCFG:");
    }
    else
    {
        at_printf("%sERROR:%d\r\n", "+MQTTCFG:", resultNo);
    }
    ATCMD_NEWLINE_HASHTAG();
    return;
}

/****************************************************************
Function            fATQR
Brief                   MQTT reset, If you forget the procedure, or how many connections you have created, you can reset all.
****************************************************************/
void fATQR(void *arg)
{
    int i = 0, res = 0;
    MQTT_CONTROL_BLOCK  *mqttCb = NULL;
    MQTT_AT_HANDLE *mqttAtHandle = &gMqttAtHandle;

    mqtt_debug("\r\n[ATQR] reset all mqtt connections");
    /* No need any other parameters. */
    for(; MQTT_MAX_CLIENT_NUM > i; i++)
    {
        mqttCb = mqttAtHandle->mqttCb[i];
        if(NULL != mqttCb)
        {
            /* Stop the task. */
            if(MQTT_TASK_START == mqttCb->taskState)
            {
                mqttCb->taskState = MQTT_TASK_STOP;
                while(MQTT_TASK_NOT_CREATE != mqttCb->taskState)
                {
                    vTaskDelay(20);
                }
            }

            /* Disconnect client. */
            if(mqttCb->client.isconnected)
            {
                mqtt_debug("\r\n[ATQR] Still connected");
                res = MQTTDisconnect(&mqttCb->client);
                if(0 != res)
                {
                    mqtt_debug("\r\n[ATQR] Can not disconnect");
                    /* Continue to stop tasks, do not break here. */
                }
                mqttCb->client.isconnected = 0;
            }

            /* Disconnect host. */
            if(mqttCb->networkConnect)
            {
                if(NULL != mqttCb->network.disconnect)
                {
                    mqtt_debug("\r\n[ATQR] Disconnect from %s", mqttCb->host);
                    mqttCb->network.disconnect(&mqttCb->network);
                }
                mqttCb->networkConnect = 0;
            }

            // NetworkDeinit(&mqttCb->network);
            mqttCb->network.my_socket = -1;
            mqttCb->network.mqttread = NULL;
            mqttCb->network.mqttwrite = NULL;
            mqttCb->network.disconnect = NULL;
#if MQTT_OVER_SSL
            mqttCb->network.use_ssl = 0;
            mqttCb->network.ssl = NULL;
#if CONFIG_USE_MBEDTLS
            mqttCb->network.conf = NULL;
#endif
#endif

            /* Clean the Client Control block. */
            mqttDelHandleCb(&mqttCb);
            mqttSetHandleCb(i, mqttCb);
        }
    }

    /* This command only output "OK". */
    at_printf("%sOK\r\n", "+MQTTRESET:");
    ATCMD_NEWLINE_HASHTAG();
}

log_item_t at_mqtt_items[ ] = {
    {"+MQTTOPEN", fATQO,{NULL,NULL}},    /* MQTT open. */
    {"+MQTTCLOSE", fATQC,{NULL,NULL}},    /* MQTT close. */
    {"+MQTTCONN", fATQN,{NULL,NULL}},    /* MQTT connect. */
    {"+MQTTDISCONN", fATQD,{NULL,NULL}},    /* MQTT disconnect. */
    {"+MQTTSUB", fATQS,{NULL,NULL}},    /* MQTT subscribe. */
    {"+MQTTUNSUB", fATQU,{NULL,NULL}},    /* MQTT unsubscribe. */
    {"+MQTTPUB", fATQP,{NULL,NULL}},    /* MQTT publish. */
    {"+MQTTCFG", fATQF,{NULL,NULL}},     /* MQTT configure. */
    {"+MQTTRESET", fATQR,{NULL,NULL}}     /* MQTT reset. */
};

/* Get the Client Control block for tcpConnId.
If the needInit == 1, we should initialise this Control block no matter it was initialised already. */
static MQTT_RESULT_ENUM mqttGetHandleCb(u8 tcpConnId, MQTT_CONTROL_BLOCK **mqttCb, u8 needInit)
{
    MQTT_RESULT_ENUM resultNo = MQTT_OK;
    MQTT_AT_HANDLE *mqttAtHandle = &gMqttAtHandle;

    if(NULL == mqttCb || MQTT_MAX_CLIENT_NUM <= tcpConnId)
    {
        mqtt_debug("\r\n[mqttGetHandleCb] Invalid parameters.");
        resultNo = MQTT_COMMON_ERROR;
        goto end;
    }

    /* Release older one. */
    if(1 == needInit && NULL != mqttAtHandle->mqttCb[tcpConnId])
    {
        mqtt_debug("\r\n[mqttGetHandleCb] Cover the older information for %d", tcpConnId);
        mqttDelHandleCb(&mqttAtHandle->mqttCb[tcpConnId]);
    }

    if(NULL == mqttAtHandle->mqttCb[tcpConnId])
    {
        /* Need to initialised. */
        if(1 == needInit)
        {
            resultNo = mqttInitHandleCb(mqttCb);
            if(MQTT_OK != resultNo)
            {
                goto end;
            }
            mqttAtHandle->mqttCb[tcpConnId] = *mqttCb;
        }
        /* If this is not the first time used. */
        else
        {
            mqtt_debug("\r\n[mqttGetHandleCb] NULL param.");
            resultNo = MQTT_MEMORY_ERROR;
            *mqttCb = NULL;
        }
    }
    /* Get it directly. */
    else
    {
        *mqttCb = mqttAtHandle->mqttCb[tcpConnId];
    }

end:
    return resultNo;
}

/* Remove the Client Control block, free the malloced memories. */
static void mqttDelHandleCb(MQTT_CONTROL_BLOCK **mqttCb)
{
    s8 i = 0;
    MQTT_CONTROL_BLOCK *pMqttCb = NULL;

    if(NULL == mqttCb || NULL == *mqttCb)
    {
        goto end;
    }
    pMqttCb = *mqttCb;

    mqttDelClientBuf(&pMqttCb->client);

    /* Delete memory malloced. */
    if(NULL != pMqttCb->connectData.will.message.cstring)
    {
        free(pMqttCb->connectData.will.message.cstring);
        pMqttCb->connectData.will.message.cstring = NULL;
    }
    if(NULL != pMqttCb->connectData.will.topicName.cstring)
    {
        free(pMqttCb->connectData.will.topicName.cstring);
        pMqttCb->connectData.will.topicName.cstring = NULL;
    }

    /* Connection information and open information. */
    if(NULL != pMqttCb->clientId)
    {
        free(pMqttCb->clientId);
    }
    if(NULL != pMqttCb->userName)
    {
        free(pMqttCb->userName);
    }
    if(NULL != pMqttCb->password)
    {
        free(pMqttCb->password);
    }
    if(NULL != pMqttCb->host)
    {
        free(pMqttCb->host);
    }
    for(i = 0; MAX_MESSAGE_HANDLERS > i; i++)
    {
        if(NULL != pMqttCb->topic[i])
        {
            free(pMqttCb->topic[i]);
        }
    }
    if(NULL != pMqttCb->pubData.topic)
    {
        free(pMqttCb->pubData.topic);
    }
    if(NULL != pMqttCb->pubData.msg)
    {
        free(pMqttCb->pubData.msg);
    }
    free(pMqttCb);
    *mqttCb = NULL;

end:
    return;
}

static void mqttSetHandleCb(u8 tcpConnId, MQTT_CONTROL_BLOCK *mqttCb)
{
    if(MQTT_MAX_CLIENT_NUM > tcpConnId)
    {
        gMqttAtHandle.mqttCb[tcpConnId] = mqttCb;
    }
}

static MQTT_RESULT_ENUM mqttInitHandleCb(MQTT_CONTROL_BLOCK **pMqttCb)
{
    MQTT_CONTROL_BLOCK *mqttCb = NULL;
    MQTT_RESULT_ENUM resultNo = MQTT_OK;

    /* Malloc memory for this block. */
    mqttCb = (MQTT_CONTROL_BLOCK *)malloc(sizeof(MQTT_CONTROL_BLOCK));
    if(NULL == mqttCb)
    {
        mqtt_debug("\r\n[mqttInitHandleCb] Memory failure.");
        resultNo = MQTT_MEMORY_ERROR;
        goto end;
    }

    /* Initialise. */
    memset(mqttCb, 0, sizeof(MQTT_CONTROL_BLOCK));
    memcpy(&mqttCb->connectData, &gMqttDefaultConnData, sizeof(MQTTPacket_connectData));
    mqttCb->port = MQTT_DEFAULT_PORT;
    mqttCb->pubData.qos = MQTT_DEFAULT_QOS;
    mqttCb->pubData.retain = MQTT_DEFAULT_RETAIN;

    NetworkInit(&mqttCb->network);
    resultNo = mqttInitClientBuf(&mqttCb->client, &mqttCb->network, MQTT_CMD_PKT_TIMEOUT_MS);
    if(MQTT_OK != resultNo)
    {
        mqtt_debug("\r\n[mqttInitHandleCb] Init client failure.");
        goto end;
    }
#if MQTT_NEW_MSG_CB
    mqttCb->client.cb = (void *)mqttCb;
#endif

end:
    if(MQTT_OK != resultNo)
    {
        if(NULL != mqttCb)
        {
            free(mqttCb);
        }
        *pMqttCb = NULL;
    }
    else
    {
        *pMqttCb = mqttCb;
    }
    return resultNo;
}

/* Similar with MQTTClientInit( ), but the buf and readbuf can be malloced at first. */
static MQTT_RESULT_ENUM mqttInitClientBuf(MQTTClient *client, Network *network, u32 timeout)
{
    u32 i = 0;
    MQTT_RESULT_ENUM resultNo = MQTT_OK;

    client->ipstack = network;

    for(; MAX_MESSAGE_HANDLERS > i; i++)
    {
        client->messageHandlers[i].topicFilter = 0;
    }
    client->defaultMessageHandler = mqttMessageArrived;
    client->command_timeout_ms = timeout;
#if MQTT_SENDBUF_ENABLE
    /* Initialise buf to NULL at first. */
    client->buf = NULL;
    client->buf_size = 0;
    client->readbuf = NULL;
    client->readbuf_size = 0;
#else
    /*  Create default buf, will be realloced when get larger msg && recover after being processed. */
    client->buf = (u8 *)malloc(sizeof(u8) * MQTT_DEFAULT_SENDBUF_SIZE);
    if(NULL == client->buf)
    {
        resultNo = MQTT_MEMORY_ERROR;
        goto end;
    }
    client->buf_size = MQTT_DEFAULT_SENDBUF_SIZE;
    client->readbuf = (u8 *)malloc(sizeof(u8) * MQTT_DEFAULT_SENDBUF_SIZE);
    if(NULL == client->readbuf)
    {
        resultNo = MQTT_MEMORY_ERROR;
        goto end;
    }
    client->readbuf_size = MQTT_DEFAULT_SENDBUF_SIZE;
#endif

    client->isconnected = 0;
    client->ping_outstanding = 0;
    client->next_packetid = 1;
    client->mqttstatus = MQTT_START;
    client->ipstack->m2m_rxevent = 0;
    TimerInit(&client->cmd_timer);
    TimerInit(&client->ping_timer);

end:
    if(MQTT_OK != resultNo)
    {
        if(NULL != client->buf)
        {
            free(client->buf);
            client->buf = NULL;
            client->buf_size = 0;
        }
        if(NULL != client->readbuf)
        {
            free(client->readbuf);
            client->readbuf = NULL;
            client->readbuf_size = 0;
        }
    }
    return resultNo;
}

/* Corresponding to mqttInitClientBuf( ). */
static void mqttDelClientBuf(MQTTClient *client)
{
    if(NULL != client)
    {
#if (!MQTT_SENDBUF_ENABLE)
        if(NULL != client->buf)
        {
            free(client->buf);
            client->buf = NULL;
            client->buf_size = 0;
        }
        if(NULL != client->readbuf)
        {
            free(client->readbuf);
            client->readbuf = NULL;
            client->readbuf_size = 0;
        }
#endif
        memset(client, 0, sizeof(MQTTClient));
    }
}

static void mqttMessageArrived(MessageData* data, void *param)
{
    MQTT_CONTROL_BLOCK *mqttCb = (MQTT_CONTROL_BLOCK *)param;
    char *uns_buff = NULL;
    unsigned int len_out = 0, data_shift = 0, total_data_len=0;
    char num_buf[8];
    unsigned short id = 0;
    u8 tcpconnectid = 0;
    char *topicdest = NULL,*topicsrc = NULL;
    int topiclen = 0;

    if(NULL == mqttCb)
    {
        mqtt_debug("\r\n[mqttMessageArrived] Nothing to do.");
        goto end;
    }

    id = data->message->id;
    tcpconnectid = mqttCb->tcpConnectId;

    if(NULL != data->topicName->cstring)
    {
        topicsrc = data->topicName->cstring;
        topiclen = strlen(topicsrc);
    }
    else
    {
        topicsrc = data->topicName->lenstring.data;
        topiclen = data->topicName->lenstring.len;
    }

    topicdest = (char *)malloc(topiclen + 1);
    if(NULL == topicdest)
    {
        mqtt_debug("\r\n[mqttMessageArrived] Memory Failure for topicdest.");
        goto end;
    }
    memset(topicdest, 0, topiclen + 1);
    strncpy(topicdest, topicsrc, topiclen);

    data_shift = strlen("%s:%d,%d,%s,") + strlen("+MQTTRECV") - 2	+ sprintf(num_buf, "%d", tcpconnectid) - 2 + sprintf(num_buf, "%d", id) - 2 + strlen(topicdest) - 2;
    len_out = data_shift + data->message->payloadlen + 1;
    total_data_len = strlen(",%d,%d,%d,%d") + sprintf(num_buf, "%d", data->message->payloadlen) - 2 + sprintf(num_buf, "%d", data->message->qos) - 2
                            + sprintf(num_buf, "%d", data->message->dup) - 2 + sprintf(num_buf, "%d", data->message->retained) - 2 + len_out;

    uns_buff = (char *)malloc(total_data_len);
    if(NULL == uns_buff)
    {
        mqtt_debug("\r\n[mqttMessageArrived] Memory Failure for uns_buff.");
        goto end;
    }
    memset(uns_buff, 0, total_data_len);

    snprintf(uns_buff, len_out, "%s:%d,%d,%s,", "+MQTTRECV",tcpconnectid,id,topicdest);
    memcpy((uns_buff + data_shift), (char *)data->message->payload, (int16_t)(data->message->payloadlen));
    snprintf(uns_buff+strlen(uns_buff), total_data_len-strlen(uns_buff), ",%d,%d,%d,%d", data->message->payloadlen,data->message->qos,data->message->dup,data->message->retained);

    at_printf("%s\r\n", uns_buff);
    ATCMD_NEWLINE_HASHTAG();

end:
    if(NULL != topicdest)
    {
        free(topicdest);
    }
    if(NULL != uns_buff)
    {
        free(uns_buff);
    }
}

static MQTT_RESULT_ENUM mqttStringCopy(char **dest, char *src, size_t sz)
{
    MQTT_RESULT_ENUM resultNo = MQTT_OK;

    if(NULL == src)
    {
        mqtt_debug("\r\n[mqttStringCopy] ARGS error.");
        resultNo = MQTT_ARGS_ERROR;
        goto end;
    }

    /* Delete the elder string. */
    if(NULL != *dest)
    {
        free(*dest);
    }
    /* sz + 1, the 1 is used for '\0'. */
    *dest = (char *)malloc(sizeof(char) * (sz + 1));
    if(NULL == *dest)
    {
        mqtt_debug("\r\n[mqttStringCopy] Memory failure.");
        resultNo = MQTT_MEMORY_ERROR;
        goto end;
    }
    _strcpy(*dest, src);
    (*dest)[sz] = '\0';

end:
    if(MQTT_OK != resultNo && NULL != *dest)
    {
        free(*dest);
        *dest = NULL;
    }
    return resultNo;
}

static void mqttClientAliveFail(MQTT_CONTROL_BLOCK *mqttCb)
{
    int i = 0;

    if(NULL == mqttCb)
    {
        mqtt_debug("\r\n[mqttClientAliveFail] Invalid param");
        return;
    }

    if(1 == mqttCb->client.isconnected)
    {
        mqttCb->client.isconnected = 0;
        mqttCb->offline = 1;
        for(i = 0; MAX_MESSAGE_HANDLERS > i; i++)
        {
            if(NULL != mqttCb->topic[i])
            {
                free(mqttCb->topic[i]);
                mqttCb->topic[i] = NULL;
            }
        }
        if(0 == mqttCb->initialConnect)
        {
            at_printf("%sOK\r\n", "+MQTTUNREACH:");
            ATCMD_NEWLINE_HASHTAG();
        }
        else
        {
            /* Do not use at_printf here. */
            mqtt_debug("\r\n[mqttClientAliveFail] Can not connect.");
        }
    }
    mqttCb->client.ping_outstanding = 0;
    mqttCb->client.next_packetid = 1;

    if(mqttCb->networkConnect)
    {
        if(NULL != mqttCb->network.disconnect)
        {
            mqttCb->network.disconnect(&mqttCb->network);
        }
        mqttCb->networkConnect = 0;
    }
    mqtt_debug("\r\n[mqttClientAliveFail] Set to MQTT_START");
    mqttCb->client.mqttstatus = MQTT_START;
}

/* Process the received data.
 The res has different meaning with resultNo.
 The resultNo is used for AT command output. The res is used for set mqtt status. */
static MQTT_RESULT_ENUM mqttClientDataProc(MQTT_CONTROL_BLOCK *mqttCb, fd_set *read_fds, u8 *needAtOutpput)
{
    s16 packet_type = 0;
    s32 res = 0, mqttStatus = 0, mqttRxEvent = 0, tmp = 0;
    Timer timer;
    MQTT_RESULT_ENUM resultNo = MQTT_OK;

    if(NULL == mqttCb || NULL == read_fds)
    {
        mqtt_debug("\r\n[mqttClientDataProc] Invalid params");
        *needAtOutpput = 1;
        resultNo = MQTT_COMMON_ERROR;
        goto end;
    }

    *needAtOutpput = 0;
    mqttStatus = mqttCb->client.mqttstatus;
    mqttRxEvent = (0 <= mqttCb->client.ipstack->my_socket) ? FD_ISSET(mqttCb->client.ipstack->my_socket, read_fds) : 0;

    if(MQTT_START == mqttStatus)
    {
        /* Once disconnected, set offline = 1, and release each topic. */
        if(mqttCb->client.isconnected)
        {
            mqttCb->client.isconnected = 0;
            mqttCb->offline = 1;
            for(int i = 0; MAX_MESSAGE_HANDLERS > i; i++)
            {
                if(NULL != mqttCb->topic[i])
                {
                    free(mqttCb->topic[i]);
                    mqttCb->topic[i] = NULL;
                }
            }
            if(0 == mqttCb->initialConnect)
            {
                at_printf("%sOK\r\n", "+MQTTUNREACH:");
                ATCMD_NEWLINE_HASHTAG();
            }
            else
            {
                /* Do not use at_printf here. */
                mqtt_debug("\r\n[mqttClientDataProc] Lost connection.");
            }
        }
        /* Try re-connect per second, except when initial-connect. */
        if(0 == mqttCb->initialConnect && 1 == mqttCb->offline && NULL != mqttCb->host && NULL != mqttCb->clientId)
        {
            vTaskDelay(1000);
            if(RTW_SUCCESS == wifi_is_ready_to_transceive(RTW_STA_INTERFACE))
            {
                if(0 >= mqttCb->network.my_socket && 1 == mqttCb->networkConnect && NULL != mqttCb->network.disconnect)
                {
                    mqttCb->network.disconnect(&mqttCb->network);
                    NetworkInit(&mqttCb->network);
                    mqttCb->networkConnect = 0;
                }
                res = NetworkConnect(&mqttCb->network, mqttCb->host, mqttCb->port);
                if(0 != res)
                {
                    // mqtt_debug("\r\n[mqttClientDataProc] Reconnect failed");
                    resultNo = MQTT_CONNECTION_ERROR;
                    goto end;
                }
                mqttCb->networkConnect = 1;
                TimerInit(&mqttCb->client.cmd_timer);
                TimerCountdownMS(&mqttCb->client.cmd_timer, mqttCb->client.command_timeout_ms);
                res = MQTTConnect(&mqttCb->client, &mqttCb->connectData);
                if(0 != res)
                {
                    // mqtt_debug("\r\n[mqttClientDataProc] Reconnect MQTT failed");
                    resultNo = MQTT_CONNECTION_ERROR;
                    goto end;
                }
                mqttCb->client.mqttstatus = MQTT_CONNECT;
                mqttCb->offline = 0;
                mqtt_debug("\r\n[mqttClientDataProc] Reconnect the network OK");
                at_printf("%sOK\r\n", "+MQTTREACH:");
                ATCMD_NEWLINE_HASHTAG();
            }
        }
        goto end;
    }

    if(mqttRxEvent)
    {
         mqttCb->client.ipstack->m2m_rxevent = 0;
         TimerInit(&timer);
         TimerCountdownMS(&timer, 1000);
         packet_type = readPacket(&mqttCb->client, &timer);
         if(CONNECT > packet_type || DISCONNECT < packet_type)
         {
            // mqtt_debug("\r\n[mqttClientDataProc] Invalid packet %d", packet_type);
            goto end;
         }
         mqtt_debug("\r\n[mqttClientDataProc] Read packet %d", packet_type);
    }

    switch(mqttStatus)
    {
        /* MQTT_CONNECT status. */
        case MQTT_CONNECT:
            if(CONNACK == packet_type)
            {
                u8 connack_rc = 255, sessionPresent = 0;
                tmp = MQTTDeserialize_connack(&sessionPresent, &connack_rc, mqttCb->client.readbuf, mqttCb->client.readbuf_size);
                if(1 == tmp)
                {
                    res = connack_rc;
                    mqtt_debug("\r\n[mqttClientDataProc] MQTT connected OK");
                }
                else
                {
                    mqtt_debug("\r\n[mqttClientDataProc] MQTT can not connect");
                    res = FAILURE;
                }
                mqttCb->initialConnect = 0;
                *needAtOutpput = 1;
            }
            else if(PINGRESP == packet_type)
            {
                mqttCb->client.ping_outstanding = 0;
            }
            else if(TimerIsExpired(&mqttCb->client.cmd_timer))
            {
                keepalive(&mqttCb->client);
            }
            if(FAILURE == res)
            {
                mqtt_debug("\r\n[mqttClientDataProc] MQTT connected ERROR (%d)", mqttCb->client.isconnected);
                if(1 == mqttCb->client.isconnected)
                {
                    res = MQTTDisconnect(&mqttCb->client);
                    if(0 != res)
                    {
                        mqtt_debug("\r\n[mqttClientDataProc] Disconnect ERROR");
                    }
                    mqttCb->client.isconnected = 0;
                }
                mqttCb->client.mqttstatus = MQTT_START;
            }
            break;

        /* MQTT_SUBTOPIC status. */
        case MQTT_SUBTOPIC:
            if(SUBACK == packet_type)
            {
                s32 count = 0, grantedQoS = -1;
                s32 i = 0;
                u16 mypacketid = 0;
                tmp = MQTTDeserialize_suback(&mypacketid, 1, &count, &grantedQoS, mqttCb->client.readbuf, mqttCb->client.readbuf_size);
                if(1 == tmp)
                {
                    /* It may be 0, 1, 2 or 0x80. */
                    res = grantedQoS;
                    mqtt_debug("\r\n[mqttClientDataProc] grantedQoS = %d", res);
                }
                if(0x80 != res)
                {
#if 1   /* FIXME! */
                    for(i = 0; MAX_MESSAGE_HANDLERS > i; i++)
                    {
                        if(NULL != mqttCb->topic[i] && mqttCb->client.messageHandlers[i].topicFilter != mqttCb->topic[i])
                        {
                            mqttCb->client.messageHandlers[i].topicFilter = mqttCb->topic[i];
                            mqttCb->client.messageHandlers[i].fp = mqttMessageArrived;
                        }
                    }
#endif
                    res = 0;
                    *needAtOutpput = 1;
                    mqttCb->client.mqttstatus = MQTT_RUNNING;
                }
            }
            else if(UNSUBACK == packet_type)
            {
                mqtt_debug("\r\n[mqttClientDataProc] Unsubscribe_end OK");
                mqttCb->client.mqttstatus = MQTT_CONNECT;
                *needAtOutpput = 1;
            }
            else if(TimerIsExpired(&mqttCb->client.cmd_timer))
            {
                *needAtOutpput = 1;
                mqtt_debug("\r\n[mqttClientDataProc] MQTT subscribe timeout");
                resultNo = MQTT_WAITACK_TIMEOUT_ERROR;
                res = FAILURE;
            }
            if(FAILURE == res)
            {
                mqtt_debug("\r\n[mqttClientDataProc] MQTT subscribe ERROR");
                mqttCb->client.mqttstatus = MQTT_START;
            }
            break;

        /* MQTT_RUNNING status. */
        case MQTT_RUNNING:
            if(CONNECT <= packet_type)
            {
                s32 len = 0;
                TimerInit(&timer);
                TimerCountdownMS(&timer, 10000);
                switch(packet_type)
                {
                    case SUBACK:
                        {
                            s32 count = 0, grantedQoS = -1;
                            s32 i = 0;
                            u16 mypacketid = 0;
                            tmp = MQTTDeserialize_suback(&mypacketid, 1, &count, &grantedQoS, mqttCb->client.readbuf, mqttCb->client.readbuf_size);
                            if(1 == tmp)
                            {
                                /* It may be 0, 1, 2 or 0x80. */
                                res = grantedQoS;
                                mqtt_debug("\r\n[mqttClientDataProc] grantedQoS = %d", res);
                            }
                            if(0x80 != res)
                            {
#if 1   /* FIXME! */
                                for(i = 0; MAX_MESSAGE_HANDLERS > i; i++)
                                {
                                    if(NULL != mqttCb->topic[i] && mqttCb->client.messageHandlers[i].topicFilter != mqttCb->topic[i])
                                    {
                                        mqttCb->client.messageHandlers[i].topicFilter = mqttCb->topic[i];
                                        mqttCb->client.messageHandlers[i].fp = mqttMessageArrived;
                                    }
                                }
#endif
                                res = 0;
                                *needAtOutpput = 1;
                                // mqttCb->client.mqttstatus = MQTT_RUNNING;
                            }
                        }
                        break;

                    case PUBACK:
                        {
                            u16 mypacketid = 0;
                            u8 dup = 0, type = 0;
                            tmp = MQTTDeserialize_ack(&type, &dup, &mypacketid, mqttCb->client.readbuf, mqttCb->client.readbuf_size);
                            if(1 != tmp)
                            {
                                mqtt_debug("\r\n[mqttClientDataProc] Deserialize_ack failed");
                                res = FAILURE;
                                resultNo = MQTT_PUBLISH_ERROR;
                            }
                            *needAtOutpput = 1;
                        }
                        break;

                    case UNSUBACK:
                        {
                            u8 i = 0;
                            mqtt_debug("\r\n[mqttClientDataProc] Unsubscribe OK");
                            for(; MAX_MESSAGE_HANDLERS > i && NULL == mqttCb->topic[i]; i++)
                            {
                                ;
                            }
                            /* !FIXME:  If there is(are) any topic(s) remained, stay in MQTT_RUNNING. */
                            mqttCb->client.mqttstatus = (MAX_MESSAGE_HANDLERS == i) ? MQTT_CONNECT : MQTT_RUNNING;
                            *needAtOutpput = 1;
                        }
                        break;

                    case PUBLISH:
                        {
                            MQTTString topicName;
                            MQTTMessage msg;
                            s32 intQoS = 0;
                            tmp = MQTTDeserialize_publish(&msg.dup, &intQoS, &msg.retained, &msg.id, &topicName,
                                        (u8 **)&msg.payload, (s32 *)&msg.payloadlen, mqttCb->client.readbuf, mqttCb->client.readbuf_size);
                            if(1 != tmp)
                            {
                                mqtt_debug("\r\n[mqttClientDataProc] Deserialize PUBLISH failed");
                                goto end;
                            }
                            msg.qos = intQoS;
                            deliverMessage(&mqttCb->client, &topicName, &msg);
                            /* QOS0 has no ack. */
                            if(QOS0 != msg.qos)
                            {
                                if(QOS1 == msg.qos)
                                {
                                    len = MQTTSerialize_ack(mqttCb->client.buf, mqttCb->client.buf_size, PUBACK, 0, msg.id);
                                    mqtt_debug("\r\n[mqttClientDataProc] Send PUBACK");
                                }
                                else if(QOS2 == msg.qos)
                                {
                                    len = MQTTSerialize_ack(mqttCb->client.buf, mqttCb->client.buf_size, PUBREC, 0, msg.id);
                                    mqtt_debug("\r\n[mqttClientDataProc] Send PUBREC");
                                }
                                else
                                {
                                    mqtt_debug("\r\n[mqttClientDataProc] Invalid QoS %d", msg.qos);
                                    goto end;
                                }
                                /* Send failed. */
                                if(0 >= len)
                                {
                                    mqtt_debug("\r\n[mqttClientDataProc] Serialize_ack failed");
                                    goto end;
                                }
                                tmp = sendPacket(&mqttCb->client, len, &timer);
                                if(FAILURE == tmp)
                                {
                                    mqtt_debug("\r\n[mqttClientDataProc] send packet failed, set to start");
                                    mqttCb->client.mqttstatus = MQTT_START;
                                    goto end;
                                }
                            }
                        }
                        break;

                    case PUBREC:
                        {
                            u16 mypacketid = 0;
                            u8 dup = 0, type = 0;
                            *needAtOutpput = 1;
                            tmp = MQTTDeserialize_ack(&type, &dup, &mypacketid, mqttCb->client.readbuf, mqttCb->client.readbuf_size);
                            if(1 != tmp)
                            {
                                mqtt_debug("\r\n[mqttClientDataProc] Deserialize PUBREC failed");
                                resultNo = MQTT_PUBLISH_ERROR;
                            }
                            else
                            {
                                len = MQTTSerialize_ack(mqttCb->client.buf, mqttCb->client.buf_size, PUBREL, 0, mypacketid);
                                if(0 >= len)
                                {
                                    mqtt_debug("\r\n[mqttClientDataProc] Serialize PUBREL failed");
                                    resultNo = MQTT_PUBLISH_ERROR;
                                }
                                else
                                {
                                    tmp = sendPacket(&mqttCb->client, len, &timer);
                                    if(FAILURE == tmp)
                                    {
                                        mqtt_debug("\r\n[mqttClientDataProc] send packet failed");
                                        resultNo = MQTT_PUBLISH_ERROR;
                                        mqttCb->client.mqttstatus = MQTT_START;
                                    }
                                }
                            }
                        }
                        break;

                    case PUBREL:
                        {
                            u16 mypacketid = 0;
                            u8 dup = 0, type = 0;
                            tmp = MQTTDeserialize_ack(&type, &dup, &mypacketid, mqttCb->client.readbuf, mqttCb->client.readbuf_size);
                            if(1 != tmp)
                            {
                                mqtt_debug("\r\n[mqttClientDataProc] Deserialize PUBREL failed");
                                res = FAILURE;
                            }
                            else
                            {
                                len = MQTTSerialize_ack(mqttCb->client.buf, mqttCb->client.buf_size, PUBCOMP, 0, mypacketid);
                                if(0 >= len)
                                {
                                    mqtt_debug("\r\n[mqttClientDataProc] Serialize PUBCOMP failed");
                                    res = FAILURE;
                                }
                                else
                                {
                                    tmp = sendPacket(&mqttCb->client, len, &timer);
                                    if(FAILURE == tmp)
                                    {
                                        mqtt_debug("\r\n[mqttClientDataProc] sent packet ERROR");
                                        res = FAILURE;
                                        mqttCb->client.mqttstatus = MQTT_START;
                                    }
                                }
                            }
                        }
                        break;

                    case PINGRESP:
                        mqttCb->client.ping_outstanding = 0;
                        break;

                    /* Ignore the other types. */
                    default:
                        break;
                }
            }
            keepalive(&mqttCb->client);
            break;

        /* The other status, ignored. */
        default:
            break;
    }

end:
    return resultNo;
}

static u8 mqttConfigGetIndex(char *cmdString)
{
    u8 i = 0;

    for(; MQTT_CONFIG_CMD_NUM > i; i++)
    {
        if(0 == _strcmp(gMqttConfigStr[i], cmdString))
        {
            return i;
        }
    }

    return i;
}

/* There are 4 connection IDs at most. */
void initMqttInfo(void)
{
    u8 i = 0;
    MQTT_AT_HANDLE *mqttAtHandle = &gMqttAtHandle;

    for(; MQTT_MAX_CLIENT_NUM > i; i++)
    {
        mqttAtHandle->mqttCb[i] = NULL;
    }
}

#if ATCMD_VER == ATVER_2
void print_mqtt_at(void *arg)
{
    int i, cmdSize;

    cmdSize = sizeof(at_mqtt_items) / sizeof(log_item_t);
    for(i = 0; cmdSize > i; i++)
    {
        at_printf("AT%s\r\n", at_mqtt_items[i].log_cmd);
    }
}
#endif

void at_mqtt_init(void)
{
    initMqttInfo();

    log_service_add_table(at_mqtt_items, sizeof(at_mqtt_items)/sizeof(at_mqtt_items[0]));
}

#if SUPPORT_LOG_SERVICE
log_module_init(at_mqtt_init);
#endif

#endif /* CONFIG_MQTT_EN */
