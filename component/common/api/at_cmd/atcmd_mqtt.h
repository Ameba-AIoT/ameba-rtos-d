#ifndef __ATCMD_MQTT_H__
#define __ATCMD_MQTT_H__

#if defined(__cplusplus)
extern "C" {
#endif

#include "main.h"
#include "lwip_netconf.h"
//#include "../../mbed/hal_ext/crypto_api.h"
#include "MQTTClient.h"
#include "MQTTPublish.h"
#include "MQTTFreertos.h"
#include "MQTTConnect.h"

#if ((defined CONFIG_MQTT_EN) && (1 == CONFIG_MQTT_EN))

/* There are 4 connection IDs at most. */
#define MQTT_MAX_CLIENT_NUM     4

#define MQTT_DEFAULT_PORT           1883

#define MQTT_DEFAULT_QOS               2    /* QOS2 */

#define MQTT_DEFAULT_RETAIN         0

#define MQTT_DEFAULT_SENDBUF_SIZE        1401

#define MQTT_SELECT_TIMEOUT         1

#define MQTT_EXCEPT_FDS                     1

#define MQTT_CONNECT_LATER          1

/* Host name's length (excluding '\0'). */
#define MQTT_MAX_HOSTNAME_LEN   1000

/* Client ID's length  (excluding '\0').*/
#define MQTT_MAX_CLIENT_ID_LEN      1000

/* username's length  (excluding '\0'). */
#define MQTT_MAX_USERNAME_LEN       1000

/* password's length  (excluding '\0'). */
#define MQTT_MAX_PASSWORD_LEN       1000

/* topic's length  (excluding '\0'). */
#define MQTT_MAX_TOPIC_LEN          1000

/* msg's length  (excluding '\0'). */
#define MQTT_MAX_MSG_LEN            (MQTT_DEFAULT_SENDBUF_SIZE - 1)

#define ATCMD_MQTT_TASK_PRIORITY    (tskIDLE_PRIORITY + 4)

/* Default timeout == 60s. */
#define MQTT_CMD_PKT_TIMEOUT_MS     60000

typedef enum MQTT_CONFIG_CMD_TYPES_e
{
    MQTT_CONFIG_VERSION = 0,
    MQTT_CONFIG_KEEPALIVE,
    MQTT_CONFIG_SESSION,
    MQTT_CONFIG_TIMEOUT,
    MQTT_CONFIG_WILL,
    MQTT_CONFIG_SSL,
    MQTT_CONFIG_CMD_NUM
}
MQTT_CONFIG_CMD_TYPES;

/* This enum may be modified later. */
typedef enum MQTT_RESULT_ENUM_e
{
    MQTT_OK = 0,
    MQTT_COMMON_ERROR,
    MQTT_ARGS_ERROR,
    MQTT_ALREADY_EXIST_ID,
    MQTT_MEMORY_ERROR,
    MQTT_NOT_ATTACH_ERROR,
    MQTT_NOT_OPEN_ERROR,
    MQTT_CONNECTION_ERROR,
    MQTT_AUTH_ERROR,
    MQTT_CLIENTID_REJECTED_ERROR,
    MQTT_NOT_CONNECTED_ERROR,
    MQTT_ALREADY_CONNECTED_ERROR,
    MQTT_PUBLISH_ERROR,
    MQTT_SUBSCRIPTION_ERROR,
    MQTT_ALREADY_SUBSCRIBED_ERROR,
    MQTT_NOT_SUBSCRIBED_ERROR,
    MQTT_UNSUBSCRIPTION_ERROR,
    MQTT_WAITACK_TIMEOUT_ERROR,
    MQTT_THREAD_CREATE_ERROR,
    MQTT_NETWORK_LINK_ERROR
}
MQTT_RESULT_ENUM;

typedef enum MQTT_CLIENT_TASK_e
{
    MQTT_TASK_NOT_CREATE = 0,
    MQTT_TASK_START,
    MQTT_TASK_STOP
}
MQTT_CLIENT_TASK;

typedef struct MQTT_PUB_DATA_t
{
    u8 qos;
    u8 retain;
    char *topic;
    char *msg;
}
MQTT_PUB_DATA;

typedef struct MQTT_CONTROL_BLOCK_t
{
    u8          tcpConnectId;   /* 0 ~ 3 */
    char        *host;
    u16         port;           /* 1 ~ 65535 */
#if MQTT_OVER_SSL
    u8          useSsl;
#endif
    char        *clientId;
    char        *userName;
    char        *password;
    /* topic[i] corresponds to client.messageHandlers[i]. */
    char        *topic[MAX_MESSAGE_HANDLERS];
    MQTT_PUB_DATA   pubData;
    u8          networkConnect;
    u8          offline;        /* Set to 1 when offline during connecting status. */
    u8          initialConnect; /* Set to 1 between MQTTCONN and CONN_ACK. */
    Network         network;
    MQTTClient  client;
    MQTTPacket_connectData  connectData;
    xTaskHandle         taskHandle;
    MQTT_CLIENT_TASK    taskState;
}
MQTT_CONTROL_BLOCK;

typedef struct MQTT_AT_HANDLE_t
{
    MQTT_CONTROL_BLOCK  *mqttCb[MQTT_MAX_CLIENT_NUM];
}
MQTT_AT_HANDLE;

#define ENABLE_MQTT_DEBUG   1
#if ENABLE_MQTT_DEBUG
#define mqtt_debug(fmt, args...)   do{printf(fmt, ##args);}while(0)
#else
#define mqtt_debug(fmt, args...)
#endif

#if 0
#if (defined(CONFIG_EXAMPLE_UART_ATCMD) && (CONFIG_EXAMPLE_UART_ATCMD))
extern char at_string[LOG_SERVICE_BUFLEN];
#define at_output(fmt, args...)     do{\
                                snprintf(at_string, LOG_SERVICE_BUFLEN, fmt, ##args);\
                                uart_at_send_string(at_string);\
                                }while(0)
#elif (defined(CONFIG_EXAMPLE_SPI_ATCMD) && (CONFIG_EXAMPLE_SPI_ATCMD))
extern char at_string[LOG_SERVICE_BUFLEN];
#define at_output(fmt,args...)      do{\
                                snprintf(at_string, LOG_SERVICE_BUFLEN, fmt, ##args);\
                                spi_at_send_string(at_string);\
                                }while(0)
#else
#define at_output(fmt,args...)      do{printf(fmt, ##args);}while(0)
#endif
#endif

extern void mqttMain(void *param);

extern MQTT_AT_HANDLE gMqttAtHandle;
extern MQTTPacket_connectData gMqttDefaultConnData;

#endif /* CONFIG_MQTT_EN */

#if defined(__cplusplus)
}
#endif

#endif /* __ATCMD_MQTT_H__ */
