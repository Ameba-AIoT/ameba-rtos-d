#include "example_captive_portal.h"

#ifdef CONFIG_READ_FLASH

#if (CONFIG_PLATFORM_AMEBA_X == 0)

#include <flash/stm32_flash.h>
#if defined(STM32F2XX)
#include <stm32f2xx_flash.h>
#elif defined(STM32F4XX)
#include <stm32f4xx_flash.h>
#elif defined(STM32f1xx)
#include <stm32f10x_flash.h>
#endif

#else
#include "flash_api.h"
#include "device_lock.h"
#define DATA_SECTOR     AP_SETTING_SECTOR
#undef BACKUP_SECTOR
#define BACKUP_SECTOR	(FLASH_SYSTEM_DATA_ADDR-0x1000)

#endif
#endif
/* ------------------------ Defines --------------------------------------- */
/* The size of the buffer in which the dynamic WEB page is created. */
#define webMAX_PAGE_SIZE		(6144/* 2800 */) /*FSL: buffer containing array*/
#define LOCAL_BUF_SIZE			1024
#define AP_SETTING_ADDR			AP_SETTING_SECTOR
/* Standard GET response. */
#define webHTTP_OK  "HTTP/1.0 200 OK\r\nContent-type: text/html\r\n\r\n"

/* The port on which we listen. */
#define webHTTP_PORT			( 80 )

/* Delay on close error. */
#define webSHORT_DELAY			( 10 )

#define USE_DIV_CSS 1

#if USE_DIV_CSS

/* Format of the dynamic page that is returned on each connection. */
#define webHTML_HEAD_START \
"<html>\
<head>\
"
/*
<meta http-equiv=\"Content-Type\" content=\"text/html;charset=gb2312>\
<meta http-equiv=\"Cache-Control\" CONTENT=\"no-cache\">\
<meta http-equiv=\"Expires\" CONTENT=\"0\">\
*/

#define webHTML_TITLE \
"<title>Realtek SoftAP Config UI</title>"

#define webHTML_BODY_START \
"</head>\
<body  onLoad=\"onChangeSecType()\">\
<form name=\"form1\" method=\"post\" onSubmit=\"return onSubmitForm()\" accept-charset=\"utf-8\">\
<div class=\"wrapper\">\
<div class=\"header\">\
Realtek SoftAP Configuration\
</div>"

#define webHTML_CSS \
"<style>\
body {\
text-align:center;\
font-family: 'Segoe UI';\
}\
.wrapper {\
text-align:left;\
margin:0 auto;\
margin-top:200px;\
border:#000;\
width:500px;\
}\
.header {\
background-color:#CF9;\
font-size:18px;\
line-height:50px;\
text-align:center;\
}\
.oneline {\
width:100%;\
border-left:#FC3 10px;\
font-size:15px;\
height:30px;\
margin-top:3px;\
}\
.left {\
background-color:#FF0;\
line-height:30px;\
width:40%;\
height:100%;\
float:left;\
padding-left:20px;\
}\
.right {\
margin-left:20px;\
}\
\
.box {\
width:40%;\
height:28px;\
margin-left:20px;\
\
}\
\
.btn {\
background-color:#CF9;\
height:40px;\
text-align:center;\
}\
\
.btn input {\
font-size:16px;\
height:30px;\
width:150px;\
border:0px;\
line-height:30px;\
margin-top:5px;\
border-radius:20px;\
color:#000;\
}\
.btn input:hover{\
cursor:pointer;\
background-color:#FB4044;\
}\
\
.foot {\
text-align:center;\
font-size:15px;\
line-height:20px;\
border:#CCC;\
}\
#pwd {\
display:none;\
}\
</style>"

#define webHTML_SUBMIT_BTN	\
" <div class=\"oneline btn\">\
<input  type=\"submit\" value=\"Submit\" style=\"background-color: #FFF;\">\
</div>\
"

#define webHTML_STA_BOBY_START	\
" <div class=\"header\">\
Realtek STA WIFI Configuration\
</div>\
"

#define webHTML_SCAN_BTN \
" <div class=\"oneline btn\">\
<input type=\"submit\" name=\"scan\" value=\"Scan\" style=\"background-color: #FFF;\">\
<input type=\"button\" name=\"connect\" value=\"Connect\" style=\"background-color: #FFF;\" onClick=\"onChangeWifiStatus()\">\
<input type=\"submit\" name=\"refresh\" value=\"Refresh\" style=\"background-color: #FFF;\">\
</div>\
"

#define webHTML_PASSWORD	\
"<div class=\"oneline\" id=\"spwd\"><div class=\"left\">Password: </div>\
<div class=\"right\" >\
<input  class=\"box\" id=\"spwd_val\" type=\"text\" name=\"SPassword\" value=\"""\" >\
 </div></div>\
"

#define webHTML_END \
" <div class=\"oneline foot\">\
Copyright &copy;realtek.com\
</div>\
 </div>\
 </form>\
</body>\
</html>\
"

#define webWaitHTML_START \
"<html location.href='wait.html'>\
<head>\
"
#define webWaitHTML_END \
"</head>\
<BODY>\
<p>\
<h2>SoftAP is now restarting!</h2>\
<h2>Please wait a moment and reconnect!</h2>\
</p>"\
"</BODY>\r\n" \
"</html>"

#define onChangeSecType \
"<script>\
function onChangeSecType()\
{\
x=document.getElementById(\"sec\");\
y=document.getElementById(\"pwd\");\
if(x.value == \"open\"){\
y.style.display=\"none\";\
}else{\
y.style.display=\"block\";\
}\
}\
</script>"

#define onSubmitForm \
"<script>\
function onSubmitForm()\
{\
x=document.getElementById(\"Ssid\");\
y=document.getElementById(\"pwd\");\
z=document.getElementById(\"pwd_val\");\
if(x.value.length>32)\
{\
alert(\"SoftAP SSID is too long!(1-32)\");\
return false;\
}\
/*if(!(/^[A-Za-z0-9]+$/.test(x.value)))\
{\
alert(\"SoftAP SSID can only be [A-Za-z0-9]\");\
return false;\
}*/\
if(y.style.display == \"block\")\
{\
if((z.value.length < 8)||(z.value.length>64))\
{\
alert(\"Password length is between 8 to 64\");\
return false;\
}\
}\
}\
</script>"

#define onChangeColor \
"<script>\
function onChangeColor() {\
elementbtn = document.getElementById(\"led_btn\");\
elementlbl = document.getElementById(\"led_val\");\
if(elementbtn.value=='OFF')\
{\
elementbtn.value='ON';\
elementlbl.value='ON';\
elementbtn.style.backgroundColor = \"#00FF00\";\
}\
else\
{\
elementbtn.value='OFF';\
elementlbl.value='OFF';\
elementbtn.style.backgroundColor = \"#C0C0C0\";\
}\
document.form1.submit();\
}\
</script>\
"

#define onChangeWifiStatus \
"<script>\
function onChangeWifiStatus() {\
targetap = document.getElementById(\"sap_val\");\
if(targetap.value==\"""\")\
{\
alert(\"TargetAP is NULL!\");\
}\
else\
{\
elementlbl = document.getElementById(\"sconn_val\");\
elementlbl.value='Connecting';\
document.form1.submit();\
}\
}\
</script>\
"

#else

/* Format of the dynamic page that is returned on each connection. */
#define webHTML_HEAD_START \
"<html>\
<head>\
"
/*
<meta http-equiv=\"Content-Type\" content=\"text/html;charset=gb2312>\
<meta http-equiv=\"Cache-Control\" CONTENT=\"no-cache\">\
<meta http-equiv=\"Expires\" CONTENT=\"0\">\
*/

#define webHTML_BODY_START \
"</head>\
<BODY onLoad=\"onChangeSecType()\">\
\r\n\r\n<form name=\"form\" method=\"post\" onsubmit=\"return onSubmitForm()\" accept-charset=\"utf-8\">\
<table width=\"500\">\
<tr>\
<td colspan=\"2\" style=\"background-color:#FFA500;text-align:center;\">\
<h2>Realtek SoftAP Configuration</h2>\
</td>\
</tr>"

#define webHTML_END \
"<tr>\
<td colspan=\"2\" style=\"background-color:#FFD700;text-align:center;height:40px\">\
<input type=\"submit\" value=\"Submit\"><br></td>\
</tr>\
<tr>\
<td colspan=\"2\" style=\"background-color:#FFA500;text-align:center;\">\
Copyright ?realtek.com</td>\
</tr>\
</table>\
\r\n</form>" \
"</BODY>\r\n" \
"</html>"

#define webWaitHTML_START \
"<html location.href='wait.html'>\
<head>\
"
#define webWaitHTML_END \
"</head>\
<BODY>\
<p>\
<h2>SoftAP is now restarting!</h2>\
<h2>Please wait a moment and reconnect!</h2>\
</p>"\
"</BODY>\r\n" \
"</html>"

#define onChangeSecType \
"<script>\
function onChangeSecType()\
{\
x=document.getElementById(\"sec\");\
y=document.getElementById(\"pwd_row\");\
if(x.value == \"open\"){\
y.style.display=\"none\";\
}else{\
y.style.display=\"block\";\
}\
}\
</script>"

#define onSubmitForm \
"<script>\
function onSubmitForm()\
{\
x=document.getElementById(\"Ssid\");\
y=document.getElementById(\"pwd_row\");\
z=document.getElementById(\"pwd\");\
if(x.value.length>32)\
{\
alert(\"SoftAP SSID is too long!(1-32)\");\
return false;\
}\
if(!(/^[A-Za-z0-9]+$/.test(x.value)))\
{\
alert(\"SoftAP SSID can only be [A-Za-z0-9]\");\
return false;\
}\
if(y.style.display == \"block\")\
{\
if((z.value.length < 8)||(z.value.length>64))\
{\
alert(\"Password length is between 8 to 64\");\
return false;\
}\
}\
}\
</script>"

#endif


/*
alert(\"Please enter your password!\");\
return false;\
}\
if(z.value.length < 8)\
{\
alert(\"Your password is too short!(8-64)\");\
return false;\
}\
if(z.value.length>64)\
{\
alert(\"Your password is too long!(8-64)\");\
*/

#define MAX_SOFTAP_SSID_LEN		32
#define MAX_PASSWORD_LEN		64
#define MAX_CHANNEL_NUM			13

rtw_wifi_setting_t wifi_setting = {RTW_MODE_NONE, {0}, 0, RTW_SECURITY_OPEN, {0}, 0};
rtw_wifi_setting_t target_ap_setting = {RTW_MODE_NONE, {0}, 0, RTW_SECURITY_OPEN, {0}, 0};

#if INCLUDE_uxTaskGetStackHighWaterMark
static volatile unsigned portBASE_TYPE uxHighWaterMark_web = 0;
#endif

/* ------------------------ Prototypes ------------------------------------ */
static void     vProcessConnection(void *param);

/*------------------------------------------------------------------------------*/
static void LoadWifiSetting(void)
{
	const char *ifname = WLAN0_NAME;

	if (rltk_wlan_running(WLAN1_IDX)) {
		//STA_AP_MODE
		ifname = WLAN1_NAME;
	}

	wifi_get_setting(ifname, &wifi_setting);

	//printf("\r\nLoadWifiSetting(): Wifi_Setting.ssid=%s\n", Wifi_Setting.ssid);
	//printf("\r\nLoadWifiSetting(): Wifi_Setting.channel=%d\n", Wifi_Setting.channel);
	//printf("\r\nLoadWifiSetting(): Wifi_Setting.security_type=%d\n", Wifi_Setting.security_type);
	//printf("\r\nLoadWifiSetting(): Wifi_Setting.password=%s\n", Wifi_Setting.password);
}

#if CONFIG_READ_FLASH
#if (CONFIG_PLATFORM_AMEBA_X == 0)
static void LoadWifiConfig(void)
{
	rtw_wifi_config_t local_config;
	uint32_t address;
#ifdef STM32F10X_XL
	address = 0x08080000;   //bank2 domain
#else
	uint16_t sector_nb = FLASH_Sector_11;
	address = flash_SectorAddress(sector_nb);
#endif
	printf("\r\nLoadWifiConfig(): Read from FLASH!\n");
	flash_Read(address, (char *)&local_config, sizeof(local_config));

	printf("\r\nLoadWifiConfig(): local_config.boot_mode=0x%x\n", local_config.boot_mode);
	printf("\r\nLoadWifiConfig(): local_config.ssid=%s\n", local_config.ssid);
	printf("\r\nLoadWifiConfig(): local_config.channel=%d\n", local_config.channel);
	printf("\r\nLoadWifiConfig(): local_config.security_type=%d\n", local_config.security_type);
	printf("\r\nLoadWifiConfig(): local_config.password=%s\n", local_config.password);

	if (local_config.boot_mode == 0x77665502) {
		wifi_setting.mode = RTW_MODE_AP;
		if (local_config.ssid_len > 32) {
			local_config.ssid_len = 32;
		}
		memcpy(wifi_setting.ssid, local_config.ssid, local_config.ssid_len);
		wifi_setting.ssid[local_config.ssid_len] = '\0';
		wifi_setting.channel = local_config.channel;
		wifi_setting.security_type = local_config.security_type;
		if (local_config.password_len > 64) {
			local_config.password_len = 64;
		}
		memcpy(wifi_setting.password, local_config.password, local_config.password_len);
		wifi_setting.password[local_config.password_len] = '\0';
	} else {
		LoadWifiSetting();
	}
}

int StoreApInfo(void)
{
	rtw_wifi_config_t wifi_config;
	uint32_t address;
#ifdef STM32F10X_XL
	address = 0x08080000;	//bank2 domain
#else
	uint16_t sector_nb = FLASH_Sector_11;
	address = flash_SectorAddress(sector_nb);
#endif
	// clean wifi_config first
	memset(&wifi_config, 0x00, sizeof(rtw_wifi_config_t));

	wifi_config.boot_mode = 0x77665502;
	memcpy(wifi_config.ssid, wifi_setting.ssid, strlen((char *)wifi_setting.ssid));
	wifi_config.ssid_len = strlen((char *)wifi_setting.ssid);
	wifi_config.security_type = wifi_setting.security_type;
	memcpy(wifi_config.password, wifi_setting.password, strlen((char *)wifi_setting.password));
	wifi_config.password_len = strlen((char *)wifi_setting.password);
	wifi_config.channel = wifi_setting.channel;

	printf("\n\rWritting boot mode 0x77665502 and Wi-Fi setting to flash ...");
#ifdef STM32F10X_XL
	FLASH_ErasePage(address);
#else
	flash_EraseSector(sector_nb);
#endif
	flash_Wrtie(address, (char *)&wifi_config, sizeof(rtw_wifi_config_t));

	return 0;
}


#else

void LoadWifiConfig(void)
{
	flash_t flash;

	rtw_wifi_config_t local_config;
	uint32_t address;

	address = DATA_SECTOR;

	//memset(&local_config,0,sizeof(rtw_wifi_config_t));
	printf("\r\nLoadWifiConfig(): Read from FLASH!\n");
	// flash_Read(address, &local_config, sizeof(local_config));

	device_mutex_lock(RT_DEV_LOCK_FLASH);
	flash_stream_read(&flash, address, sizeof(rtw_wifi_config_t), (uint8_t *)(&local_config));
	device_mutex_unlock(RT_DEV_LOCK_FLASH);

	printf("\r\nLoadWifiConfig(): local_config.boot_mode=0x%x\n", local_config.boot_mode);
	printf("\r\nLoadWifiConfig(): local_config.ssid=%s\n", local_config.ssid);
	printf("\r\nLoadWifiConfig(): local_config.channel=%d\n", local_config.channel);
	printf("\r\nLoadWifiConfig(): local_config.security_type=%d\n", local_config.security_type);
	printf("\r\nLoadWifiConfig(): local_config.password=%s\n", local_config.password);

	if (local_config.boot_mode == 0x77665502) {
		wifi_setting.mode = RTW_MODE_AP;
		if (local_config.ssid_len > 32) {
			local_config.ssid_len = 32;
		}
		memcpy(wifi_setting.ssid, local_config.ssid, local_config.ssid_len);
		wifi_setting.ssid[local_config.ssid_len] = '\0';
		wifi_setting.channel = local_config.channel;
		if (local_config.security_type == 1) {
			wifi_setting.security_type = RTW_SECURITY_WPA2_AES_PSK;
		} else {
			wifi_setting.security_type = RTW_SECURITY_OPEN;
		}
		if (local_config.password_len > 64) {
			local_config.password_len = 64;
		}
		memcpy(wifi_setting.password, local_config.password, local_config.password_len);
		wifi_setting.password[local_config.password_len] = '\0';
	} else {
		LoadWifiSetting();
	}

}

int StoreApInfo(void)
{

	flash_t flash;

	rtw_wifi_config_t wifi_config;
	uint32_t address;
	uint32_t data, i = 0;

	// clean wifi_config first
	memset(&wifi_config, 0x00, sizeof(rtw_wifi_config_t));

	address = DATA_SECTOR;

	wifi_config.boot_mode = 0x77665502;
	memcpy(wifi_config.ssid, wifi_setting.ssid, strlen((char *)wifi_setting.ssid));
	wifi_config.ssid_len = strlen((char *)wifi_setting.ssid);
	wifi_config.security_type = wifi_setting.security_type;
	if (wifi_setting.security_type != 0) {
		wifi_config.security_type = 1;
	} else {
		wifi_config.security_type = 0;
	}
	memcpy(wifi_config.password, wifi_setting.password, strlen((char *)wifi_setting.password));
	wifi_config.password_len = strlen((char *)wifi_setting.password);
	wifi_config.channel = wifi_setting.channel;
	printf("\n\rWritting boot mode 0x77665502 and Wi-Fi setting to flash ...");
	//printf("\n\r &wifi_config = 0x%x",&wifi_config);

	flash_read_word(&flash, address, &data);

	if (data == ~0x0UL) {
		device_mutex_lock(RT_DEV_LOCK_FLASH);
		flash_stream_write(&flash, address, sizeof(rtw_wifi_config_t), (uint8_t *)&wifi_config);
		device_mutex_unlock(RT_DEV_LOCK_FLASH);
	} else {
		//flash_EraseSector(sector_nb);

		device_mutex_lock(RT_DEV_LOCK_FLASH);
		flash_erase_sector(&flash, BACKUP_SECTOR);
		for (i = 0; i < 0x1000; i += 4) {
			flash_read_word(&flash, DATA_SECTOR + i, &data);
			if (i < sizeof(rtw_wifi_config_t)) {
				memcpy(&data, (char *)(&wifi_config) + i, 4);
				//printf("\n\r Wifi_config + %d = 0x%x",i,(void *)(&wifi_config + i));
				//printf("\n\r Data = %d",data);
			}
			flash_write_word(&flash, BACKUP_SECTOR + i, data);
		}
		flash_read_word(&flash, BACKUP_SECTOR + 68, &data);
		//printf("\n\r Base + BACKUP_SECTOR + 68 wifi channel = %d",data);
		//erase system data
		flash_erase_sector(&flash, DATA_SECTOR);
		//write data back to system data
		for (i = 0; i < 0x1000; i += 4) {
			flash_read_word(&flash, BACKUP_SECTOR + i, &data);
			flash_write_word(&flash, DATA_SECTOR + i, data);
		}
		//erase backup sector
		flash_erase_sector(&flash, BACKUP_SECTOR);
	}
	device_mutex_unlock(RT_DEV_LOCK_FLASH);
	//flash_Wrtie(address, (char *)&wifi_config, sizeof(rtw_wifi_config_t));
	//flash_stream_write(&flash, address,sizeof(rtw_wifi_config_t), (uint8_t *)&wifi_config);
	//flash_stream_read(&flash, address, sizeof(rtw_wifi_config_t),data);
	//flash_stream_read(&flash, address, sizeof(rtw_wifi_config_t),data);
	//printf("\n\r Base + 0x000FF000 +4  wifi config  = %s",data[4]);
	//printf("\n\r Base + 0x000FF000 +71 wifi channel = %d",data[71]);

	return 0;
}

int EraseApinfo(void)
{
	flash_t flash;
	uint32_t address;

	address = DATA_SECTOR;
	device_mutex_lock(RT_DEV_LOCK_FLASH);
	flash_erase_sector(&flash, address);
	device_mutex_unlock(RT_DEV_LOCK_FLASH);
	return 0;
}
#endif

#endif

static void RestartSoftAP(void)
{
	//printf("\r\nRestartAP: ssid=%s", wifi_setting.ssid);
	//printf("\r\nRestartAP: ssid_len=%d", strlen((char*)Wifi_Setting.ssid));
	//printf("\r\nRestartAP: security_type=%d", wifi_setting.security_type);
	//printf("\r\nRestartAP: password=%s", wifi_setting.password);
	//printf("\r\nRestartAP: password_len=%d", strlen((char*)Wifi_Setting.password));
	//printf("\r\nRestartAP: channel=%d\n", wifi_setting.channel);
	wifi_restart_ap(wifi_setting.ssid,
					wifi_setting.security_type,
					wifi_setting.password,
					strlen((char *)wifi_setting.ssid),
					strlen((char *)wifi_setting.password),
					wifi_setting.channel);
}

static u32 web_atoi(char *s)
{
	int num = 0, flag = 0;
	u32 i;

	for (i = 0; i <= strlen(s); i++) {
		if (s[i] >= '0' && s[i] <= '9') {
			num = num * 10 + s[i] - '0';
		} else if (s[0] == '-' && i == 0) {
			flag = 1;
		} else {
			break;
		}
	}

	if (flag == 1) {
		num = num * -1;
	}

	return (num);
}

static void CreateSsidTableItem(char *pbuf, u8_t *ssid, u8_t ssid_len)
{
	char local_ssid[MAX_SOFTAP_SSID_LEN + 1];

	if (ssid_len > MAX_SOFTAP_SSID_LEN) {
		ssid_len = MAX_SOFTAP_SSID_LEN;
	}
	memcpy(local_ssid, ssid, ssid_len);
	local_ssid[ssid_len] = '\0';

#if USE_DIV_CSS
	sprintf(pbuf, "<div class=\"oneline\"><div class=\"left\">SoftAP SSID:</div> <div class=\"right\">" \
			"<input class=\"box\" type=\"text\" name=\"Ssid\" id=\"Ssid\" value=\"%s\"></div></div>",
			local_ssid);
#else
	sprintf(pbuf, "<tr>"
			"<td style=\"background-color:#FFD700;width:100px;\">"
			"<b>SoftAP SSID:</b><br>"
			"</td>"
			"<td style=\"background-color:#eeeeee;height:30px;width:400px;\">"
			"<input type=\"text\" name=\"Ssid\" id=\"Ssid\" value=\"%s\"><br>"
			"</td>"
			"</tr>",
			local_ssid);

#endif
	//printf("\r\nstrlen(SsidTableItem)=%d\n", strlen(pbuf));
}

static void CreateSecTypeTableItem(char *pbuf, u32_t sectype)
{
	u8_t flag[2] = {0, 0};

	if (sectype == RTW_SECURITY_OPEN) {
		flag[0] = 1;
	} else if (sectype == RTW_SECURITY_WPA2_AES_PSK) {
		flag[1] = 1;
	} else {
		return;
	}

#if USE_DIV_CSS
	sprintf(pbuf, "<div class=\"oneline\"><div class=\"left\">Security Type: </div><div class=\"right\">"\
			"<select  class=\"box\" name=\"Security Type\"  id=\"sec\" onChange=onChangeSecType()>"\
			"<option value=\"open\" %s>OPEN</option><option value=\"wpa2-aes\" %s>WPA2-AES</option>"\
			"</select></div></div>",
			flag[0] ? "selected" : "",
			flag[1] ? "selected" : "");
#else
	sprintf(pbuf, "<tr>"
			"<td style=\"background-color:#FFD700;width:100px;\">"
			"<b>Security Type:</b><br>"
			"</td>"
			"<td style=\"background-color:#eeeeee;height:30px;\">"
			"<select name=\"Security Type\"  id=\"sec\" onChange=onChangeSecType()>"
			"<option value=\"open\" %s>OPEN</option>"
			"<option value=\"wpa2-aes\" %s>WPA2-AES</option>"
			"</select>"
			"</td>"
			"</tr>",
			flag[0] ? "selected" : "",
			flag[1] ? "selected" : "");
#endif

	//printf("\r\nstrlen(SecTypeTableItem)=%d\n", strlen(pbuf));
}

static void CreatePasswdTableItem(char *pbuf, u8_t *password, u8_t passwd_len)
{
	char local_passwd[MAX_PASSWORD_LEN + 1];

	if (passwd_len > MAX_PASSWORD_LEN) {
		passwd_len = MAX_PASSWORD_LEN;
	}
	if (passwd_len > 0) {
		memcpy(local_passwd, password, passwd_len);
		local_passwd[passwd_len] = '\0';
	}

#if USE_DIV_CSS

	sprintf(pbuf,	"<div class=\"oneline\" id=\"pwd\"><div class=\"left\">Password: </div>"\
			"<div class=\"right\" >"\
			"<input  class=\"box\" id=\"pwd_val\" type=\"text\" name=\"Password\" value=\"%s\" >"\
			" </div></div>",
			passwd_len ? local_passwd : "");
#else
	sprintf(pbuf, "<tr id=\"pwd_row\">"
			"<td style=\"background-color:#FFD700;width:100px;\">"
			"<b>Password:</b><br>"
			"</td>"
			"<td style=\"background-color:#eeeeee;height:30px;\">"
			"<input type=\"text\" name=\"Password\" id=\"pwd\" value=\"%s\" ><br>"
			"</td>"
			"</tr>",
			passwd_len ? local_passwd : "");
#endif
	//printf("\r\nstrlen(passwordTableItem)=%d\n", strlen(pbuf));
}

static void CreateChannelTableItem(char *pbuf, u8_t channel)
{
	u8_t flag[MAX_CHANNEL_NUM + 1] = {0};

	if (channel > MAX_CHANNEL_NUM) {
		printf("Channel(%d) is out of range!\n", channel);
		channel = 1;
	}
	flag[channel] = 1;

#if USE_DIV_CSS

	sprintf(pbuf, "<div class=\"oneline\"><div class=\"left\">Channel: </div>"
			"<div class=\"right\"><select  class=\"box\" name=\"Channel\">"
			"<option value=\"1\" %s>1</option>"
			"<option value=\"2\" %s>2</option>"
			"<option value=\"3\" %s>3</option>"
			"<option value=\"4\" %s>4</option>"
			"<option value=\"5\" %s>5</option>"
			"<option value=\"6\" %s>6</option>"
			"<option value=\"7\" %s>7</option>"
			"<option value=\"8\" %s>8</option>"
			"<option value=\"9\" %s>9</option>"
			"<option value=\"10\" %s>10</option>"
			"<option value=\"11\" %s>11</option>"
			"</select> </div> </div>",

			flag[1] ? "selected" : "",
			flag[2] ? "selected" : "",
			flag[3] ? "selected" : "",
			flag[4] ? "selected" : "",
			flag[5] ? "selected" : "",
			flag[6] ? "selected" : "",
			flag[7] ? "selected" : "",
			flag[8] ? "selected" : "",
			flag[9] ? "selected" : "",
			flag[10] ? "selected" : "",
			flag[11] ? "selected" : "");
#else
	sprintf(pbuf, "<tr>"
			"<td style=\"background-color:#FFD700;width:100px;\">"
			"<b>Channel:</b><br>"
			"</td>"
			"<td style=\"background-color:#eeeeee;height:30px;\">"
			"<select name=\"Channel\">"
			"<option value=\"1\" %s>1</option>"
			"<option value=\"2\" %s>2</option>"
			"<option value=\"3\" %s>3</option>"
			"<option value=\"4\" %s>4</option>"
			"<option value=\"5\" %s>5</option>"
			"<option value=\"6\" %s>6</option>"
			"<option value=\"7\" %s>7</option>"
			"<option value=\"8\" %s>8</option>"
			"<option value=\"9\" %s>9</option>"
			"<option value=\"10\" %s>10</option>"
			"<option value=\"11\" %s>11</option>"
			"</select>"
			"</td>"
			"</tr>",
			flag[1] ? "selected" : "",
			flag[2] ? "selected" : "",
			flag[3] ? "selected" : "",
			flag[4] ? "selected" : "",
			flag[5] ? "selected" : "",
			flag[6] ? "selected" : "",
			flag[7] ? "selected" : "",
			flag[8] ? "selected" : "",
			flag[9] ? "selected" : "",
			flag[10] ? "selected" : "",
			flag[11] ? "selected" : "");
#endif

	//printf("\r\nstrlen(ChannelTableItem)=%d\n", strlen(pbuf));
}

static void CreateTargetAPTableItem(char *pbuf)
{
	char local_passwd[MAX_PASSWORD_LEN + 1];
	char local_ssid[MAX_SOFTAP_SSID_LEN + 1];
	u8_t passwd_len, ssid_len;

	passwd_len = strlen((char *)target_ap_setting.password);
	ssid_len = strlen((char *)target_ap_setting.ssid);

	if (ssid_len > MAX_SOFTAP_SSID_LEN) {
		ssid_len = MAX_SOFTAP_SSID_LEN;
	}
	memcpy(local_ssid, target_ap_setting.ssid, ssid_len);
	local_ssid[ssid_len] = '\0';

	if (passwd_len > MAX_PASSWORD_LEN) {
		passwd_len = MAX_PASSWORD_LEN;
	}
	if (passwd_len > 0) {
		memcpy(local_passwd, target_ap_setting.password, passwd_len);
		local_passwd[passwd_len] = '\0';
	}

#if USE_DIV_CSS
	sprintf(pbuf, "<div class=\"oneline\" id=\"sap\"><div class=\"left\">TargetAP: </div>"\
			"<div class=\"right\" ><input  class=\"box\" id=\"sap_val\" type=\"text\" name=\"TargetAP\" value=\"%s\" >"\
			" </div></div>"\
			"<div class=\"oneline\" id=\"spwd\"><div class=\"left\">Password: </div>"\
			"<div class=\"right\" ><input  class=\"box\" id=\"spwd_val\" type=\"text\" name=\"SPassword\" value=\"%s\" >"\
			" </div></div>",
			ssid_len ? local_ssid : "", passwd_len ? local_passwd : "");

#else
	sprintf(pbuf, "<tr id=\"pwd_row\">"
			"<td style=\"background-color:#FFD700;width:100px;\">"
			"<b>Password:</b><br>"
			"</td>"
			"<td style=\"background-color:#eeeeee;height:30px;\">"
			"<input type=\"text\" name=\"Password\" id=\"pwd\" value=\"%s\" ><br>"
			"</td>"
			"</tr>",
			passwd_len ? local_passwd : "");
#endif
	//printf("\r\nstrlen(passwordTableItem)=%d\n", strlen(pbuf));
}

extern int wifi_is_connected_to_ap(void);
static void CreateConnStatusTableItem(char *pbuf)
{
	u8 connect_status;
	connect_status = wifi_is_connected_to_ap();

#if USE_DIV_CSS
	sprintf(pbuf, "<div class=\"oneline\" id=\"sconn\"><div class=\"left\">CONNECTION STATUS: </div>"\
			"<div class=\"right\" ><input  class=\"box\" id=\"sconn_val\" type=\"text\" name=\"CONNSTATUS\" value=\"%s\" >"\
			" </div></div>",
			connect_status ? "connect fail" : "connect success");

#else

#endif
	//printf("\r\nstrlen(passwordTableItem)=%d\n", strlen(pbuf));
}


static void CreateScanSsidTableItem(char *pbuf)
{
	u8_t flag[MAX_CHANNEL_NUM + 1] = {0};
	int ret;

	flag[0] = 1;
	flag[1] = 0;

#if USE_DIV_CSS

	ret = snprintf(pbuf, LOCAL_BUF_SIZE, "<div class=\"oneline\"><div class=\"left\">APList: </div>"\
				   "<div class=\"right\"><select  class=\"box\" name=\"APList\">"\
				   "<option value=\"%s\" %s>%s</option>"\
				   "<option value=\"%s\" %s>%s</option>"\
				   "<option value=\"%s\" %s>%s</option>"\
				   "<option value=\"%s\" %s>%s</option>"\
				   "<option value=\"%s\" %s>%s</option>"\
				   "<option value=\"%s\" %s>%s</option>"\
				   "<option value=\"%s\" %s>%s</option>"\
				   "<option value=\"%s\" %s>%s</option>"\
				   "<option value=\"%s\" %s>%s</option>"\
				   "<option value=\"%s\" %s>%s</option>"\
				   "</select> </div> </div>",
				   scan_result.ap_list[0].ssid, flag[0] ? "selected" : "", scan_result.ap_list[0].ssid ? scan_result.ap_list[0].ssid : "",
				   scan_result.ap_list[1].ssid, flag[1] ? "selected" : "", scan_result.ap_list[1].ssid ? scan_result.ap_list[1].ssid : "",
				   scan_result.ap_list[2].ssid, flag[1] ? "selected" : "", scan_result.ap_list[2].ssid ? scan_result.ap_list[2].ssid : "",
				   scan_result.ap_list[3].ssid, flag[1] ? "selected" : "", scan_result.ap_list[3].ssid ? scan_result.ap_list[3].ssid : "",
				   scan_result.ap_list[4].ssid, flag[1] ? "selected" : "", scan_result.ap_list[4].ssid ? scan_result.ap_list[4].ssid : "",
				   scan_result.ap_list[5].ssid, flag[1] ? "selected" : "", scan_result.ap_list[5].ssid ? scan_result.ap_list[5].ssid : "",
				   scan_result.ap_list[6].ssid, flag[1] ? "selected" : "", scan_result.ap_list[6].ssid ? scan_result.ap_list[6].ssid : "",
				   scan_result.ap_list[7].ssid, flag[1] ? "selected" : "", scan_result.ap_list[7].ssid ? scan_result.ap_list[7].ssid : "",
				   scan_result.ap_list[8].ssid, flag[1] ? "selected" : "", scan_result.ap_list[8].ssid ? scan_result.ap_list[8].ssid : "",
				   scan_result.ap_list[9].ssid, flag[1] ? "selected" : "", scan_result.ap_list[9].ssid ? scan_result.ap_list[9].ssid : ""
				  );
#else
	ret = sprintf(pbuf, LOCAL_BUF_SIZE, "<tr>"
				  "<td style=\"background-color:#FFD700;width:100px;\">"
				  "<b>Channel:</b><br>"
				  "</td>"
				  "<td style=\"background-color:#eeeeee;height:30px;\">"
				  "<select name=\"Channel\">"
				  "<option value=\"1\" %s>1</option>"
				  "<option value=\"2\" %s>2</option>"
				  "<option value=\"3\" %s>3</option>"
				  "<option value=\"4\" %s>4</option>"
				  "<option value=\"5\" %s>5</option>"
				  "<option value=\"6\" %s>6</option>"
				  "<option value=\"7\" %s>7</option>"
				  "<option value=\"8\" %s>8</option>"
				  "<option value=\"9\" %s>9</option>"
				  "<option value=\"10\" %s>10</option>"
				  "<option value=\"11\" %s>11</option>"
				  "</select>"
				  "</td>"
				  "</tr>",
				  flag[1] ? "selected" : "",
				  flag[2] ? "selected" : "",
				  flag[3] ? "selected" : "",
				  flag[4] ? "selected" : "",
				  flag[5] ? "selected" : "",
				  flag[6] ? "selected" : "",
				  flag[7] ? "selected" : "",
				  flag[8] ? "selected" : "",
				  flag[9] ? "selected" : "",
				  flag[10] ? "selected" : "",
				  flag[11] ? "selected" : "");
#endif

	if (ret < 0) {
		printf("%s error\n", __func__);
	}
	//printf("\r\nstrlen(ChannelTableItem)=%d\n", strlen(pbuf));
}

static void GenerateIndexHtmlPage(portCHAR *cDynamicPage, portCHAR *LocalBuf)
{
	while (xSemaphoreTake(webs_wpage_sema, portMAX_DELAY) != pdTRUE);
	/* Generate the page index.html...
	... First the page header. */
	strcpy(cDynamicPage, webHTML_HEAD_START);

	/* Add script */
	strcat(cDynamicPage, onChangeSecType);
	strcat(cDynamicPage, onSubmitForm);
#if USE_DIV_CSS
	strcat(cDynamicPage, onChangeColor);
	strcat(cDynamicPage, onChangeWifiStatus);
#endif
#if USE_DIV_CSS
	/* add css */
	strcat(cDynamicPage, webHTML_CSS);

	strcat(cDynamicPage, webHTML_TITLE);
#endif
	/* Add Body start */
	strcat(cDynamicPage, webHTML_BODY_START);

	/* Add SSID */
	CreateSsidTableItem(LocalBuf, wifi_setting.ssid, strlen((char *)wifi_setting.ssid));
	strcat(cDynamicPage, LocalBuf);

	/* Add SECURITY TYPE  */
	CreateSecTypeTableItem(LocalBuf, wifi_setting.security_type);
	strcat(cDynamicPage, LocalBuf);

	/* Add PASSWORD */
	CreatePasswdTableItem(LocalBuf, wifi_setting.password, strlen((char *)wifi_setting.password));
	strcat(cDynamicPage, LocalBuf);

	/* Add CHANNEL  */
	CreateChannelTableItem(LocalBuf, wifi_setting.channel);
	strcat(cDynamicPage, LocalBuf);

#if USE_DIV_CSS
	/* Add Sbumit button */
	strcat(cDynamicPage, webHTML_SUBMIT_BTN);

	/* Add STA body */
	strcat(cDynamicPage, webHTML_STA_BOBY_START);
#endif

#if USE_DIV_CSS
	CreateScanSsidTableItem(LocalBuf);
	strcat(cDynamicPage, LocalBuf);

	CreateTargetAPTableItem(LocalBuf);
	strcat(cDynamicPage, LocalBuf);

	CreateConnStatusTableItem(LocalBuf);
	strcat(cDynamicPage, LocalBuf);

	strcat(cDynamicPage, webHTML_SCAN_BTN);

#endif
	/* ... Finally the page footer. */
	strcat(cDynamicPage, webHTML_END);
	//printf("\r\nGenerateIndexHtmlPage(): %s\n", cDynamicPage);
	printf("\r\nGenerateIndexHtmlPage Len: %d\n", strlen(cDynamicPage));
	xSemaphoreGive(webs_wpage_sema);
}

static web_conn *web_conn_add(void)
{
	int i;
	web_conn *conn = NULL;

	for (i = 0; i < MAX_HTTP_CONNECTIONS; i++) {
		if (web_connections[i].status == -1) {
			web_connections[i].status = 1;
			conn = &web_connections[i];
#if CAPTIVE_PORTAL_DEBUG
			printf("%s connection %d\n", __FUNCTION__, i);
#endif
			break;
		}
	}
	return conn;
}

static void web_conn_remove(web_conn *conn)
{
	int i;

	for (i = 0; i < MAX_HTTP_CONNECTIONS; i++) {
		if (&web_connections[i] == conn) {
			web_connections[i].status = -1;
#if CAPTIVE_PORTAL_DEBUG
			printf("%s connection %d\n", __FUNCTION__, i);
#endif
			break;
		}
	}

}

static void web_conn_clear(void)
{
	int i;

	printf("enter %s\n", __func__);

	for (i = 0; i < MAX_HTTP_CONNECTIONS; i++) {
		if (web_connections[i].status == 1) {
#if CAPTIVE_PORTAL_DEBUG
			printf("%s connection %d, port = %d\n", __FUNCTION__, i, web_connections[i].conn->pcb.tcp->remote_port);
#endif
			netconn_close(web_connections[i].conn);
			netconn_delete(web_connections[i].conn);
			web_connections[i].status = -1;
		}
	}

}

static void GenerateWaitHtmlPage(portCHAR *cDynamicPage)
{
	while (xSemaphoreTake(webs_wpage_sema, portMAX_DELAY) != pdTRUE);
	/* Generate the dynamic page...
	... First the page header. */
	strcpy(cDynamicPage, webWaitHTML_START);

	/* ... Finally the page footer. */
	strcat(cDynamicPage, webWaitHTML_END);

	//printf("\r\nGenerateWaitHtmlPage(): %s\n",  cDynamicPage);
	//printf("\r\nGenerateWaitHtmlPage Len: %d\n", strlen( cDynamicPage ));
	xSemaphoreGive(webs_wpage_sema);
}

static int scan_result_handler(rtw_scan_handler_result_t *malloced_scan_result)
{
	static int ApNum = 0;
	if (malloced_scan_result->scan_complete != RTW_TRUE) {
		rtw_scan_result_t *record = &malloced_scan_result->ap_details;
		record->SSID.val[record->SSID.len] = 0; /* Ensure the SSID is null terminated */
		ApNum++;
		//printf("%d\t ", ++ApNum);
		//print_scan_result(record);
		if (malloced_scan_result->user_data && ApNum <= SCAN_AP_LIST_MAX) {
			rtw_memcpy((void *)((char *)malloced_scan_result->user_data + (ApNum - 1)*sizeof(rtw_scan_result_t)), (char *)record, sizeof(rtw_scan_result_t));
		}
	} else {
		int i = 0;
		int j = 0;
		if (ApNum == 0) {
			goto end;
		}
		if (ApNum > SCAN_AP_LIST_MAX) {
			ApNum = SCAN_AP_LIST_MAX;
		}

		for (i = 0; i < ApNum; i++) {
			rtw_scan_result_t *res =  malloced_scan_result->user_data + i * sizeof(rtw_scan_result_t);
			if (!strcmp(res->SSID.val, "")) {
				continue;
			}

			rtw_memcpy((scan_result.ap_list + j)->ssid, res->SSID.val, res->SSID.len);
			(scan_result.ap_list + j)->ssid[res->SSID.len] = '\0';
			rtw_memcpy((scan_result.ap_list + j)->bssid, res->BSSID.octet, 6);

			(scan_result.ap_list + j)->ap_power = res->signal_strength;
			(scan_result.ap_list + j)->channel = res->channel;
			(scan_result.ap_list + j)->sec_type = res->security;

			j++;

#if CAPTIVE_PORTAL_DEBUG
			printf(MAC_FMT, MAC_ARG(scanned_AP_info->BSSID.octet));
			printf(" %d\t ", scanned_AP_info->signal_strength);
			printf(" %d\t  ", scanned_AP_info->channel);
			printf(" %d\t  ", (unsigned int)scanned_AP_info->wps_type);
			printf("%s\t\t ", (scanned_AP_info->security == RTW_SECURITY_OPEN) ? "Open" :
				   (scanned_AP_info->security == RTW_SECURITY_WEP_PSK) ? "WEP" :
				   (scanned_AP_info->security == RTW_SECURITY_WPA_TKIP_PSK) ? "WPA TKIP" :
				   (scanned_AP_info->security == RTW_SECURITY_WPA_AES_PSK) ? "WPA AES" :
				   (scanned_AP_info->security == RTW_SECURITY_WPA_MIXED_PSK) ? "WPA Mixed" :
				   (scanned_AP_info->security == RTW_SECURITY_WPA2_AES_PSK) ? "WPA2 AES" :
				   (scanned_AP_info->security == RTW_SECURITY_WPA2_TKIP_PSK) ? "WPA2 TKIP" :
				   (scanned_AP_info->security == RTW_SECURITY_WPA2_MIXED_PSK) ? "WPA2 Mixed" :
				   (scanned_AP_info->security == RTW_SECURITY_WPA_WPA2_TKIP_PSK) ? "WPA/WPA2 TKIP" :
				   (scanned_AP_info->security == RTW_SECURITY_WPA_WPA2_AES_PSK) ? "WPA/WPA2 AES" :
				   (scanned_AP_info->security == RTW_SECURITY_WPA_WPA2_MIXED_PSK) ? "WPA/WPA2 Mixed" :
				   "Unknown		");
			printf(" %s\n", scanned_AP_info->SSID.val);
#endif
		}
		scan_result.ap_num = ApNum;
end:
		ApNum = 0;
		rtw_up_sema(&scan_done_sema);
		if (malloced_scan_result->user_data) {
			free(malloced_scan_result->user_data);
		}
	}
	return 0;
}

static int wifi_start_scan(void)
{
	printf("%s\n", __func__);
	char *scan_buf;

	scan_buf = rtw_malloc(SCAN_AP_LIST_MAX * sizeof(rtw_scan_result_t));
	if (!scan_buf) {
		printf("ERROR: malloc failed!\n");
		return -1;
	}
	rtw_memset(scan_buf, 0, SCAN_AP_LIST_MAX * sizeof(rtw_scan_result_t));
	rtw_memset(scan_result.ap_list, 0, SCAN_AP_LIST_MAX * sizeof(ap_list_t));

	if (wifi_scan_networks((rtw_scan_result_handler_t)scan_result_handler, scan_buf) != 0) {
		printf("ERROR: wifi scan failed!\n");
		return -1;
	}

	return 0;
}

static void wifi_scan_thread(void *pvParameters)
{
	(void)pvParameters;

	wifi_start_scan();
	vTaskDelete(NULL);
}


/*find ap with "ssid" from scan list*/
static int _find_ap_from_scan_buf(char *buf, int buflen, char *target_ssid, void *user_data)
{
	rtw_wifi_setting_t *pwifi = (rtw_wifi_setting_t *)user_data;
	int plen = 0;

	while (plen < buflen) {
		u8 len, ssid_len, security_mode;
		char *ssid;

		// len offset = 0
		len = (int) * (buf + plen);
		// check end
		if (len == 0) {
			break;
		}
		// ssid offset = 14
		ssid_len = len - 14;
		ssid = buf + plen + 14 ;
		if ((ssid_len == strlen(target_ssid))
			&& (!memcmp(ssid, target_ssid, ssid_len))) {
			//DBG_8195A("find a ap \r\n");
			strncpy((char *)pwifi->ssid, target_ssid, 33);
			// channel offset = 13
			pwifi->channel = *(buf + plen + 13);
			// security_mode offset = 11
			security_mode = (u8) * (buf + plen + 11);

			if (security_mode == RTW_ENCODE_ALG_NONE) {
				pwifi->security_type = RTW_SECURITY_OPEN;
			} else if (security_mode == RTW_ENCODE_ALG_WEP) {
				pwifi->security_type = RTW_SECURITY_WEP_PSK;
			} else if (security_mode == RTW_ENCODE_ALG_CCMP) {
				pwifi->security_type = RTW_SECURITY_WPA2_AES_PSK;
			}

			if (RTW_ENCODE_ALG_WEP != security_mode) {
				break;
			}
		}
		plen += len;
	}
	return 0;
}

/*get ap security mode from scan list*/
static int _get_ap_security_mode(IN char *ssid, OUT rtw_security_t *security_mode, OUT u8 *channel)
{
	rtw_wifi_setting_t wifi;
	u32 scan_buflen = 1000;

	memset(&wifi, 0, sizeof(wifi));

	if (wifi_scan_networks_with_ssid(_find_ap_from_scan_buf, (void *)&wifi, scan_buflen, ssid, strlen(ssid)) != RTW_SUCCESS) {
		printf("Wifi scan failed!\n");
		return 0;
	}

	if (strcmp(wifi.ssid, ssid) == 0) {
		//printf("Wifi scanned !\n");
		*security_mode = wifi.security_type;
		*channel = wifi.channel;
		return 1;
	}

	return 0;
}


static void ConnectTargetAP(void)
{
	int ret;

	if (rltk_wlan_channel_switch_announcement_is_enable()) {
		if (wifi_mode == RTW_MODE_STA_AP) {
			u8 connect_channel;
			int security_retry_count = 0;

			while (1) {
				if (_get_ap_security_mode((char *)target_ap_setting.ssid, &target_ap_setting.security_type, &connect_channel)) {
					break;
				}
				security_retry_count++;
				if (security_retry_count >= 3) {
					printf("Can't get AP security mode and channel.\n");
					ret = RTW_NOTFOUND;
					return;
				}
			}
			//disable wlan1 issue_deauth when channel switched by wlan0
			ret = wifi_set_ch_deauth(0);
			if (ret != 0) {
				printf("wifi_set_ch_deauth FAIL\n");
				return;
			}
			//softap switch chl and inform
			if (wifi_ap_switch_chl_and_inform(connect_channel) != RTW_SUCCESS) {
				printf("wifi_ap_switch_chl_and_inform FAIL\n");
			}
		}
	}

	printf("\n\rJoining BSS by SSID %s...\n\r", (char *)target_ap_setting.ssid);
	ret = wifi_connect((char *)target_ap_setting.ssid, target_ap_setting.security_type, (char *)target_ap_setting.password, strlen(target_ap_setting.ssid),
					   strlen(target_ap_setting.password), 0, NULL);

	if (ret != RTW_SUCCESS) {
		if (ret == RTW_INVALID_KEY) {
			printf("\n\rERROR:Invalid Key ");
		}

		printf("\n\rERROR: Can't connect to AP");
		return;
	}

#if CONFIG_LWIP_LAYER
	/* Start DHCPClient */
	LwIP_DHCP(0, DHCP_START);
#endif

}

static void http_translate_url_encode(char *ptr)
{

	char *data = ptr;
	char tmp_data[3] = {0};
	char outdata[33] = {0};
	int buffer;
	char *outdata_ptr = outdata;

	while (*data != '\0') {

		if (*data == '%') {
			if ((*(data + 1) != 0) && (*(data + 2) != 0)) {
				tmp_data[0] = *(data + 1);
				tmp_data[1] = *(data + 2);
				sscanf(tmp_data, "%x", &buffer);
				*outdata_ptr = (char)buffer;

				/* destroy data */
				*data  = 0;
				*(data + 1)  = 0;
				*(data + 2)  = 0;

				data += 2;
				outdata_ptr++;
			}

		} else {
			*outdata_ptr = *data;
			if (*data == '+') {
				*outdata_ptr = ' ';
			}
			outdata_ptr++;
		}
		data++;
	}
	strcpy(ptr, outdata);

}

static u8_t ProcessPostMessage(struct netbuf   *pxRxBuffer, portCHAR *LocalBuf)
{
	struct pbuf *p;
	portCHAR *pcRxString, *ptr;
	unsigned portSHORT usLength;
	u8_t bChanged = 0;
	rtw_security_t secType;
	u8_t channel;
	u8_t len = 0;

	pcRxString = LocalBuf;
	p = pxRxBuffer->p;
	usLength = p->tot_len;

	while (p) {
		memcpy(pcRxString, p->payload, p->len);
		pcRxString += p->len;
		p = p->next;
	}
	pcRxString = LocalBuf;
	pcRxString[usLength] = '\0';
	printf("\r\n usLength=%d pcRxString = %s\n", usLength, pcRxString);

	ptr = (char *)strstr(pcRxString, "Ssid=");
	if (ptr) {
		//printf("ssid passed = %s\n", ptr);
		pcRxString = (char *)strstr(ptr, "&");
		*pcRxString++ = '\0';
		ptr += 5;
		http_translate_url_encode(ptr);
		if (strcmp((char *)wifi_setting.ssid, ptr)) {
			bChanged = 1;
			len = strlen(ptr);
			if (len > MAX_SOFTAP_SSID_LEN) {
				len = MAX_SOFTAP_SSID_LEN;
				ptr[len] = '\0';
			}
			strcpy((char *)wifi_setting.ssid, ptr);
		}
	}

	//printf("\r\n get wifi_config.ssid = %s\n", Wifi_Setting.ssid);

	ptr = (char *)strstr(pcRxString, "Security+Type=");
	if (ptr) {
		pcRxString = (char *)strstr(ptr, "&");
		*pcRxString++ = '\0';
		ptr += 14;
		if (!strcmp(ptr, "open")) {
			secType = RTW_SECURITY_OPEN;
		} else if (!strcmp(ptr, "wpa2-aes")) {
			secType = RTW_SECURITY_WPA2_AES_PSK;
		} else {
			secType = RTW_SECURITY_OPEN;
		}
		if (wifi_setting.security_type != secType) {
			bChanged = 1;
			wifi_setting.security_type = secType;
		}
	}

	//printf("\r\n wifi_config.security_type = %d\n", Wifi_Setting.security_type);
	if (wifi_setting.security_type > RTW_SECURITY_OPEN) {
		ptr = (char *)strstr(pcRxString, "Password=");
		if (ptr) {
			pcRxString = (char *)strstr(ptr, "&");
			*pcRxString++ = '\0';
			ptr += 9;
			if (strcmp((char *)wifi_setting.password, ptr)) {
				bChanged = 1;
				len = strlen(ptr);
				if (len > MAX_PASSWORD_LEN) {
					len = MAX_PASSWORD_LEN;
					ptr[len] = '\0';
				}
				strcpy((char *)wifi_setting.password, ptr);
			}
		}
		//printf("\r\n wifi_config.password = %s\n", Wifi_Setting.password);
	}
	ptr = (char *)strstr(pcRxString, "Channel=");
	if (ptr) {
		ptr += 8;
		channel = web_atoi(ptr);
		if ((channel > MAX_CHANNEL_NUM) || (channel < 1)) {
			channel = 1;
		}
		if (wifi_setting.channel != channel) {
			bChanged = 1;
			wifi_setting.channel = channel;
		}
	}
	//printf("\r\n wifi_config.channel = %d\n", Wifi_Setting.channel);

	ptr = (char *)strstr(pcRxString, "TargetAP=");
	if (ptr) {
		pcRxString = (char *)strstr(ptr, "&");
		*pcRxString++ = '\0';
		ptr += 9;
		strcpy((char *)target_ap_setting.ssid, ptr);
	}

	ptr = (char *)strstr(pcRxString, "SPassword=");
	if (ptr) {
		pcRxString = (char *)strstr(ptr, "&");
		*pcRxString++ = '\0';
		ptr += 10;
		len = strlen(ptr);
		strcpy((char *)target_ap_setting.password, ptr);
	}

	ptr = (char *)strstr(pcRxString, "CONNSTATUS=");
	if (ptr) {
		pcRxString = (char *)strstr(ptr, "&");
		if (pcRxString) {
			*pcRxString++ = '\0';
		} else {
			pcRxString = "\0";
		}
		ptr += 11;
		if (!strcmp(ptr, "Connecting")) {
			connect_target_ap = 1;
			bChanged = 0;
		}
	}

	ptr = (char *)strstr(pcRxString, "scan=");
	if (ptr) {
		scan_ap = 1;
		bChanged = 0;
	}

	return bChanged;
}

static void vProcessConnection(void *param)
{
	static portCHAR cDynamicPage[webMAX_PAGE_SIZE];
	struct netbuf *pxRxBuffer, *pxRxBuffer1 = NULL;
	portCHAR *pcRxString;
	unsigned portSHORT usLength;
	static portCHAR LocalBuf[LOCAL_BUF_SIZE];
	u8_t bChanged = 0;
	int ret_recv = ERR_OK;
	int ret_accept = ERR_OK;
	web_conn *pxNetCon  = (web_conn *)param;
	struct task_struct conn_task = pxNetCon->task;

	/* Load WiFi Setting*/
	LoadWifiSetting();

	printf("\r\n%s %d, remote_port = %d\n", __func__, __LINE__, pxNetCon->conn->pcb.tcp->remote_port);

	pxNetCon->conn->recv_timeout = 500;

	/* We expect to immediately get data. */
	port_netconn_recv(pxNetCon->conn, pxRxBuffer, ret_recv);

	if (pxRxBuffer != NULL && ret_recv == ERR_OK) {
		/* Where is the data? */
		netbuf_data(pxRxBuffer, (void *)&pcRxString, &usLength);

		//printf("\r\nusLength=%d pcRxString = \n%s\n", usLength, pcRxString);
		/* Is this a GET?  We don't handle anything else. */
		if (!strncmp(pcRxString, "GET", 3)) {
			printf("\r\nusLength=%d pcRxString=%s \n", usLength, pcRxString);

			/* Write out the HTTP OK header. */
			netconn_write(pxNetCon->conn, webHTTP_OK, (u16_t) strlen(webHTTP_OK), NETCONN_COPY);

			/* Generate index.html page. */
			GenerateIndexHtmlPage(cDynamicPage, LocalBuf);

			//printf("\r\n cDynamicPage=%s \n", cDynamicPage);

			/* Write out the dynamically generated page. */
			netconn_write(pxNetCon->conn, cDynamicPage, (u16_t) strlen(cDynamicPage), NETCONN_COPY);
		} else if (!strncmp(pcRxString, "POST", 4)) {
			/* Write out the HTTP OK header. */
			netconn_write(pxNetCon->conn, webHTTP_OK, (u16_t) strlen(webHTTP_OK), NETCONN_COPY);

			bChanged = ProcessPostMessage(pxRxBuffer, LocalBuf);
			if (bChanged == 0) {
				port_netconn_recv(pxNetCon->conn, pxRxBuffer1, ret_recv);
				if (pxRxBuffer != NULL && ret_recv == ERR_OK) {
					bChanged = ProcessPostMessage(pxRxBuffer1, LocalBuf);
					netbuf_delete(pxRxBuffer1);
				}
			}

			if (bChanged) {
				GenerateWaitHtmlPage(cDynamicPage);

				/* Write out the generated page. */
				netconn_write(pxNetCon->conn, cDynamicPage, (u16_t) strlen(cDynamicPage), NETCONN_COPY);
#if CONFIG_READ_FLASH
				StoreApInfo();
#endif
			} else {

				if (connect_target_ap) {
					connect_target_ap = 0;
					ConnectTargetAP();
				}

				if (scan_ap) {
					scan_ap = 0;
					if (xTaskCreate(wifi_scan_thread, (const char *)"wifi_scan_thread", 1024, NULL, tskIDLE_PRIORITY + 1, NULL) != pdPASS) {
						printf("\n\rWEB: Create wifi_scan_thread task failed!\n");
					}

					rtw_down_sema(&scan_done_sema);
					printf("wifi scan done\n");
				}

				/* Generate index.html page. */
				GenerateIndexHtmlPage(cDynamicPage, LocalBuf);

				/* Write out the generated page. */
				netconn_write(pxNetCon->conn, cDynamicPage, (u16_t) strlen(cDynamicPage), NETCONN_COPY);
			}
		}
		netbuf_delete(pxRxBuffer);
	}
	netconn_close(pxNetCon->conn);

	if (bChanged) {
		struct netconn *pxNewConnection;
		RestartSoftAP();
        pxHTTPListener->recv_timeout = 1;		
		port_netconn_accept(pxHTTPListener, pxNewConnection, ret_accept);
		if (pxNewConnection != NULL && ret_accept == ERR_OK) {
			netconn_close(pxNewConnection);
			while (netconn_delete(pxNewConnection) != ERR_OK) {
				vTaskDelay(webSHORT_DELAY);
			}
		}
		pxHTTPListener->recv_timeout = 0;
	}
	while (netconn_delete(pxNetCon->conn) != ERR_OK) {
		vTaskDelay(webSHORT_DELAY);
	}
	web_conn_remove(pxNetCon);
	rtw_delete_task(&conn_task);
}

static void vCaptivePortalServer(void *pvParameters)
{
	web_conn *pxNewConnection;
	extern err_t ethernetif_init(struct netif * netif);
	int ret = ERR_OK;
	int i;
	/* Parameters are not used - suppress compiler error. */
	(void)pvParameters;

	/* Create a new tcp connection handle */
	pxHTTPListener = netconn_new(NETCONN_TCP);
	ip_set_option(pxHTTPListener->pcb.ip, SOF_REUSEADDR);
	netconn_bind(pxHTTPListener, NULL, webHTTP_PORT);
	netconn_listen(pxHTTPListener);

	web_connections = (web_conn *)rtw_malloc(sizeof(web_conn) * MAX_HTTP_CONNECTIONS);
	if (!web_connections) {
		printf("malloc web_connections failed !!");
		goto exit;
	} else {
		for (i = 0; i < MAX_HTTP_CONNECTIONS; i++) {
			web_connections[i].status = -1;
		}
	}

#if CONFIG_READ_FLASH
	/* Load wifi_config */
	LoadWifiConfig();
	RestartSoftAP();
#endif
	printf("enter vCaptivePortalServer\n");
	printf("MEMP_NUM_NETBUF = %d, MEMP_NUM_TCP_PCB =%d, MEMP_NUM_NETCONN = %d\n", MEMP_NUM_NETBUF, MEMP_NUM_TCP_PCB, MEMP_NUM_NETCONN);

	/* Loop forever */
	for (;;) {
		if (webs_terminate) {
			break;
		}

		pxNewConnection = web_conn_add();

		if (pxNewConnection) {
			/* Service connection. */
			port_netconn_accept(pxHTTPListener, pxNewConnection->conn, ret);
			if (pxNewConnection->conn != NULL && ret == ERR_OK) {
				if (rtw_create_task(&pxNewConnection->task,	(const char *)"web_conn", 1024, tskIDLE_PRIORITY + 1, vProcessConnection, pxNewConnection) != pdPASS) {
					printf("ERROR: xTaskCreate web_server_conn");
				}
			} else {
				vTaskDelay(webSHORT_DELAY);
				web_conn_remove(pxNewConnection);
			}
		}
	}
	if (pxHTTPListener) {
		netconn_close(pxHTTPListener);
		netconn_delete(pxHTTPListener);
		pxHTTPListener = NULL;
	}

	printf("stop_web_server done \n");

	for (i = 0; i < MAX_HTTP_CONNECTIONS; i++) {
		if (web_connections[i].status == 1) {
			printf("active connect port: %d\n", web_connections[i].conn->pcb.tcp->remote_port);
		}
	}

exit:
	if (web_connections) {
		rtw_free(web_connections);
	}
	xSemaphoreGive(webs_sema);
	vTaskDelete(NULL);
}

#define STACKSIZE				512

#if CONFIG_LWIP_LAYER
extern struct netif xnetif[NET_IF_NUM];
#endif

static void example_start_captive_portal(void *param)
{
	/* To avoid gcc warnings */
	(void) param;

#if CONFIG_LWIP_LAYER
	struct ip_addr ipaddr;
	struct ip_addr netmask;
	struct ip_addr gw;
	struct netif *pnetif = &xnetif[1];
#endif

	int timeout = 20;

	//vTaskDelay(4000);

#if CONFIG_LWIP_LAYER
	dhcps_deinit();
	IP4_ADDR(ip_2_ip4(&ipaddr), GW_ADDR0, GW_ADDR1, GW_ADDR2, GW_ADDR3);
	IP4_ADDR(ip_2_ip4(&netmask), NETMASK_ADDR0, NETMASK_ADDR1, NETMASK_ADDR2, NETMASK_ADDR3);
	IP4_ADDR(ip_2_ip4(&gw), GW_ADDR0, GW_ADDR1, GW_ADDR2, GW_ADDR3);
	netif_set_addr(pnetif, ip_2_ip4(&ipaddr), ip_2_ip4(&netmask), ip_2_ip4(&gw));
#endif

	printf("\r\nWEB:Enter start captive_portal!\n");

	wifi_off();

	vTaskDelay(20);

	printf("\n\rWEB: Enable Wi-Fi with STA + AP mode\n");
	if (wifi_on(RTW_MODE_STA_AP) < 0) {
		printf("\n\rWEB: wifi_on failed\n");
		return;
	}

	/*********************************************************************************
	*	1-3. Start AP
	*********************************************************************************/
	if (wifi_start_ap(SOFTAP_SSID, SOFTAP_SECURITY, SOFTAP_PASSWORD, strlen(SOFTAP_SSID), (SOFTAP_PASSWORD) ? strlen(SOFTAP_PASSWORD) : 0, SOFTAP_CHANNEL) < 0) {
		printf("\n\rWEB: wifi_start_ap failed\n");
		return;
	}
	printf("\n\rCheck AP running\n");

	while (1) {
		char essid[33];
		if (wext_get_ssid(WLAN1_NAME, (unsigned char *) essid) > 0) {
			if (strcmp((const char *) essid, (const char *)SOFTAP_SSID) == 0) {
				printf("\n\rWEB: %s started\n", SOFTAP_SSID);
				break;
			}
		}

		if (timeout == 0) {
			printf("\n\rERROR: Start AP timeout!");
			break;
		}

		vTaskDelay(1 * configTICK_RATE_HZ);
		timeout --;
	}

#if CONFIG_LWIP_LAYER
	dhcps_init(pnetif);
#endif
	rltk_wlan_enable_channel_switch_announcement(ENABLE);

	scan_result.ap_list = (ap_list_t *)malloc(SCAN_AP_LIST_MAX * sizeof(ap_list_t));
	if (scan_result.ap_list == NULL) {
		printf("ERROR: scan_result.ap_list malloc failed!\n");
		goto exit;
	}

	rtw_init_sema(&scan_done_sema, 0);
	wifi_start_scan();
	rtw_down_sema(&scan_done_sema);
	printf("wifi scan done\n");

	webs_terminate = 0;
	if (webs_task == NULL) {
		if (xTaskCreate(vCaptivePortalServer, (const char *)"web_server", STACKSIZE, NULL, tskIDLE_PRIORITY + 1, &webs_task) != pdPASS) {
			printf("\n\rWEB: Create webserver task failed!\n");
		}
	}
	if (webs_sema == NULL) {
		webs_sema = xSemaphoreCreateCounting(0xffffffff, 0);	//Set max count 0xffffffff
	}
	if (webs_wpage_sema == NULL) {
		vSemaphoreCreateBinary(webs_wpage_sema);
	}
	//printf("\r\nWEB:Exit start captive_portal!\n");
exit:
	vTaskDelete(NULL);
}

void stop_captive_portal(void)
{
	//printf("\r\nWEB:Enter stop captive_portal!\n");
	webs_terminate = 1;
	if (pxHTTPListener) {
		netconn_abort(pxHTTPListener);
	}
	if (webs_sema) {
		if (xSemaphoreTake(webs_sema, 15 * configTICK_RATE_HZ) != pdTRUE) {
			if (pxHTTPListener) {
				netconn_close(pxHTTPListener);
				netconn_delete(pxHTTPListener);
				pxHTTPListener = NULL;
			}
			printf("\r\nWEB: Take webs sema(%p) failed!!!!!!!!!!!\n", webs_sema);
		}
		vSemaphoreDelete(webs_sema);
		webs_sema = NULL;
	}
	if (webs_wpage_sema) {
		xSemaphoreTake(webs_wpage_sema, 15 * configTICK_RATE_HZ);
		vSemaphoreDelete(webs_wpage_sema);
		webs_wpage_sema = NULL;
	}
	if (webs_task) {
		vTaskDelete(webs_task);
		webs_task = NULL;
	}
	web_conn_clear();

	printf("\r\nWEB:Exit stop captive_portal!\n");
}

void example_captive_portal(void)
{
	if (xTaskCreate(example_start_captive_portal, ((const char *)"example_start_captive_portal"), 1024, NULL, tskIDLE_PRIORITY + 1, NULL) != pdPASS) {
		printf("\n\r%s xTaskCreate failed\n", __FUNCTION__);
	}
}

