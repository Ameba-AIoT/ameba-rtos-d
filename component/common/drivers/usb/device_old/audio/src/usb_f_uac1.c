#include <platform_opts.h>

#ifdef CONFIG_USBD_AUDIO

#include "usb_audio.h"
#include "usb_uac1.h"
#include "usb_audio_8763.h"
#include "usb_audio_if.h"
#include "usb_composite.h"
#include "osdep_service.h"
#include "ameba_soc.h"
#if defined (CONFIG_PLATFORM_8721D)
#include "ameba_otg.h"
#endif

#define AUDIO_FUNCTION_DESC_CNT 23// desc count

//for gaudio_snd_dev.format : 0: 16bit, 1 : 24bit
#define SOUND_FORMAT_BIT_COUNT_16 (0)
#define SOUND_FORMAT_BIT_COUNT_24 (1)

#define MAX_REQ_SIZE 1500
#define WINDOWS_MIN_VOLUME (0xDB00)

#define PCM_TABLE_CNT 8

#define UAC_FORMAT_TYPE_I_DISCRETE_DESC_SIZE(n) (8 + (n * 3))

#define UAC_FREQUENCY_BYTE_0(fre)         (fre & 0x0000ff)
#define UAC_FREQUENCY_BYTE_1(fre)         ((fre & 0x00ff00)>>8)
#define UAC_FREQUENCY_BYTE_2(fre)         ((fre & 0xff0000)>>16)
#define UAC_FREQUENCY_BYTE(fre,byte)      UAC_FREQUENCY_BYTE_##byte(fre)

#define USBD_AUDIO_STRING_MANUFACTURER        1
#define USBD_AUDIO_STRING_PRODUCT             2
#define USBD_AUDIO_STRING_SERIALNUMBER        3

struct usb_descriptor_header **audio_fs_function;
uint16_t g_req_out_buf_size;
uint16_t g_req_in_buf_size;
uint32_t *pcm_rate_table;

static struct usb_string audio_strings[] = {
    {
        .id = USBD_AUDIO_STRING_MANUFACTURER,
        .s  = "Realtek",
    },
    {
        .id = USBD_AUDIO_STRING_PRODUCT,
        .s  = "USB Audio Device",
    },
    {
        .id = USBD_AUDIO_STRING_SERIALNUMBER,
        .s  = "0123456789AB",
    },
};

static struct usb_gadget_strings audio_gadget_string_tab = {
    .language = 0x0409,
    .strings = audio_strings,
};
    
static struct usb_gadget_strings *audio_gadget_strings[] = {
    &audio_gadget_string_tab,
    NULL
};

// Audio Device Descriptor
static struct usb_device_descriptor audio_device_desc = {
    .bLength            = USB_DT_DEVICE_SIZE,
    .bDescriptorType    = USB_DT_DEVICE,
    .bcdUSB             = 0x0200,
    .bDeviceClass       = USB_CLASS_PER_INTERFACE,
    .bDeviceSubClass    = 0,
    .bDeviceProtocol    = 0,
    .bMaxPacketSize0    = 64,
    .idVendor           = REALTEK_USB_VID,
    .idProduct          = REALTEK_USB_PID,
    .bcdDevice          = 0x0100,
    .iManufacturer      = USBD_AUDIO_STRING_MANUFACTURER,
    .iProduct           = USBD_AUDIO_STRING_PRODUCT,
    .iSerialNumber      = USBD_AUDIO_STRING_SERIALNUMBER,
    .bNumConfigurations = 1,
};

static struct usb_config_descriptor audio_config_desc = {
    .bLength             = USB_DT_CONFIG_SIZE,
    .bDescriptorType     = USB_DT_CONFIG,
    .bNumInterfaces      = 1, // This will be assign automatically
    .bConfigurationValue = 1,
    .iConfiguration      = 0,
	.bmAttributes	     = USB_CONFIG_ATT_ONE | USB_CONFIG_ATT_SELFPOWER,
    .bMaxPower           = 50,
};



//ac
static struct usb_interface_descriptor ac_interface_desc0 = {
    .bLength =            sizeof ac_interface_desc0,
    .bDescriptorType =    USB_DT_INTERFACE,
    .bInterfaceNumber =   AUDIO_CONTROL_INTF_NUM, // this will be assign automatically
    .bAlternateSetting =  0x00,
    .bNumEndpoints =      0x00,
    .bInterfaceClass =    USB_CLASS_AUDIO,
    .bInterfaceSubClass = USB_SUBCLASS_AUDIOCONTROL,
    .bInterfaceProtocol = 0,
    .iInterface =         0,
};
//4.3.2 Table 4-2
static struct uac1_ac_header_descriptor ac_header_desc = {
    .bLength =            sizeof ac_header_desc,
    .bDescriptorType =    USB_DT_CS_INTERFACE,
    .bDescriptorSubtype = UAC_HEADER,
    .bcdADC =             0x0100,
    .wTotalLength =       0x64,
    .bInCollection =      F_AUDIO_NUM_INTERFACES,
    .baInterfaceNr[0] =   0x01,
    .baInterfaceNr[1] =   0x02,
};


/*
	USBIN:
	in 1[mic] -> feature1 -> select -> out 1
*/
//msg send to host
static struct uac_input_terminal_descriptor input_terminal_desc1 = {
    .bLength =            UAC_DT_INPUT_TERMINAL_SIZE,
    .bDescriptorType =    USB_DT_CS_INTERFACE,
    .bDescriptorSubtype = UAC_INPUT_TERMINAL,
    .bTerminalID =        INPUT_TERMINAL_MIC_ID,
    .wTerminalType =      UAC_INPUT_TERMINAL_MICROPHONE,
    .bAssocTerminal =     0,
    .bNrChannels =        0x01,
    .wChannelConfig =     0x1,
    .iChannelNames =      0x00,
    .iTerminal =          0x00,
};
//Table 4-7
static struct uac_feature_unit_descriptor_1 feature_unit_desc1 = {
    .bLength =            sizeof(feature_unit_desc1),
    .bDescriptorType =    USB_DT_CS_INTERFACE,
    .bDescriptorSubtype = UAC_FEATURE_UNIT,
    .bUnitID =            FEATURE_UNIT_1_ID,
    .bSourceID =          INPUT_TERMINAL_MIC_ID,
    .bControlSize =       0x01,
    .bmaControls[0] =     UAC_CONTROL_BIT(UAC_FU_AUTOMATIC_GAIN) | UAC_CONTROL_BIT(UAC_FU_MUTE) | UAC_CONTROL_BIT(UAC_FU_VOLUME),
    .bmaControls[1] =     0,
    .iFeature =           0,
};
//4.3.2.4 Table 4-6
static struct uac_selector_unit_descriptor selector_unit_desc = {
    .bLength =            7,
    .bDescriptorType =    USB_DT_CS_INTERFACE,
    .bDescriptorSubtype = UAC_SELECTOR_UNIT,
    .bUintID =            SELECTOR_UNIT_ID,
    .bNrInPins =          0x01,
    .baSourceID =         FEATURE_UNIT_1_ID,
    .iSelector =          0,
};

static struct uac1_output_terminal_descriptor output_terminal_desc1 = {
    .bLength =            UAC_DT_OUTPUT_TERMINAL_SIZE,
    .bDescriptorType =    USB_DT_CS_INTERFACE,
    .bDescriptorSubtype = UAC_OUTPUT_TERMINAL,
    .bTerminalID =        OUTPUT_TERMINAL_USB_ID,
    .wTerminalType =      UAC_TERMINAL_STREAMING,
    .bAssocTerminal =     0x00,
    .bSourceID =          SELECTOR_UNIT_ID,
    .iTerminal =          0x00,
};


/*
	USBOUT:
			in 0[2CH ID]
					      	-> Mix[mix] -> fea 0 -> out 1
	in 1 -> fea 2[fea2 ID]
*/
//4.3.2.1 Table 4-3
//msg send to device
static struct uac_input_terminal_descriptor input_terminal_desc0 = {
    .bLength =            UAC_DT_INPUT_TERMINAL_SIZE,
    .bDescriptorType =    USB_DT_CS_INTERFACE,
    .bDescriptorSubtype = UAC_INPUT_TERMINAL,
    .bTerminalID =        INPUT_TERMINAL_2CH_ID,
    .wTerminalType =      UAC_TERMINAL_STREAMING,
    .bAssocTerminal =     0,
    .bNrChannels =        0x02,
    .wChannelConfig =     0x3,
    .iChannelNames =      0x00,
    .iTerminal =          0x00,
};

static struct uac_feature_unit_descriptor_1 feature_unit_desc2 = {
    .bLength =            sizeof(feature_unit_desc2),
    .bDescriptorType =    USB_DT_CS_INTERFACE,
    .bDescriptorSubtype = UAC_FEATURE_UNIT,
    .bUnitID =            FEATURE_UNIT_2_ID,
    .bSourceID =          INPUT_TERMINAL_MIC_ID,
    .bControlSize =       0x01,
    .bmaControls[0] =     UAC_CONTROL_BIT(UAC_FU_MUTE) | UAC_CONTROL_BIT(UAC_FU_VOLUME),
    .bmaControls[1] =     0,
    .iFeature =           0,
};
//4.3.2.3 Table 4-5
static struct uac_mixer_unit_descriptor mixer_unit_desc = {
    .bLength =            sizeof(mixer_unit_desc),
    .bDescriptorType =    USB_DT_CS_INTERFACE,
    .bDescriptorSubtype = UAC_MIXER_UNIT,
    .bUnitID =            MIXER_UNIT_2_ID,
    .bNrInPins =          0x02,
	.baSourceID[0] =      INPUT_TERMINAL_2CH_ID, // in 0
	.baSourceID[1] =      FEATURE_UNIT_2_ID,     // feature 2
    // Number of logical output channels in the Mixer?s output audio channel cluster.
    .bNrChannels =        2,
    .bmChannelConfig =    0x03,    //left + right
    .iChannelNames =      0,       // unused , 0
    // mixer controlable // Bit map indicating which mixing Controls are programmable.
    .bmControls =         0,
    .iMixer =             0,       // string index
};
//4.3.2.5 Table 4-7
static struct uac_feature_unit_descriptor_2 feature_unit_desc0 = {
    .bLength =            sizeof(feature_unit_desc0),
    .bDescriptorType =    USB_DT_CS_INTERFACE,
    .bDescriptorSubtype = UAC_FEATURE_UNIT,
    .bUnitID =            FEATURE_UNIT_0_ID,
    .bSourceID =          MIXER_UNIT_2_ID,
    .bControlSize =       0x01,
	.bmaControls[0] =     UAC_CONTROL_BIT(UAC_FU_MUTE) | UAC_CONTROL_BIT(UAC_FU_VOLUME),
	.bmaControls[1] =     0,
	.bmaControls[2] =     0,
    .iFeature =           0,
};

static struct uac1_output_terminal_descriptor output_terminal_desc0 = {
    .bLength =            UAC_DT_OUTPUT_TERMINAL_SIZE,
    .bDescriptorType =    USB_DT_CS_INTERFACE,
    .bDescriptorSubtype = UAC_OUTPUT_TERMINAL,
    .bTerminalID =        OUTPUT_TERMINAL_SPEAKER_ID,
    .wTerminalType =      UAC_OUTPUT_TERMINAL_SPEAKER,
    .bAssocTerminal =     0x00,
    .bSourceID =          FEATURE_UNIT_0_ID,
    .iTerminal =          0x00,
};



//as ep out 0
/* B.4.1  Standard AS Interface Descriptor */
static struct usb_interface_descriptor as_out_interface_alt_0_desc = {
    .bLength =            USB_DT_INTERFACE_SIZE,
    .bDescriptorType =    USB_DT_INTERFACE,
    .bInterfaceNumber =   AUDIO_STREAM_OUT_INTF_NUM,
    .bAlternateSetting =  0,
    .bNumEndpoints =      0,
    .bInterfaceClass =    USB_CLASS_AUDIO,
    .bInterfaceSubClass = USB_SUBCLASS_AUDIOSTREAMING,
    .bInterfaceProtocol = 0,
    .iInterface =         0x0,
};

//as ep out alter 1
static struct usb_interface_descriptor as_out_interface_alt_1_desc = {
    .bLength =            USB_DT_INTERFACE_SIZE,
    .bDescriptorType =    USB_DT_INTERFACE,
    .bInterfaceNumber =   AUDIO_STREAM_OUT_INTF_NUM,
    .bAlternateSetting =  1,
    .bNumEndpoints =      1,
    .bInterfaceClass =    USB_CLASS_AUDIO,
    .bInterfaceSubClass = USB_SUBCLASS_AUDIOSTREAMING,
    .bInterfaceProtocol = 0,
    .iInterface =         0x00,
};
/* B.4.2  Class-Specific AS Interface Descriptor */
static struct uac1_as_header_descriptor as_out_header_desc1 = {
    .bLength =            UAC_DT_AS_HEADER_SIZE,
    .bDescriptorType =    USB_DT_CS_INTERFACE,
    .bDescriptorSubtype = UAC_AS_GENERAL,
    .bTerminalLink =      INPUT_TERMINAL_2CH_ID,
    .bDelay =             1,
    .wFormatTag =         UAC_FORMAT_TYPE_I_PCM,
};
static struct uac_format_type_i_discrete_descriptor as_out_type_i_desc1 = {
    .bLength =            UAC_FORMAT_TYPE_I_DISCRETE_DESC_SIZE(2),
    .bDescriptorType =    USB_DT_CS_INTERFACE,
    .bDescriptorSubtype = UAC_FORMAT_TYPE,
    .bFormatType =        UAC_FORMAT_TYPE_I,
    .bNrChannels =        0x2,
    	.bSubframeSize =      2,
	.bBitResolution =     16,
	.bSamFreqType =       1,
	.tSamFreq[0][0] =     UAC_FREQUENCY_BYTE(48000, 0),
	.tSamFreq[0][1] =     UAC_FREQUENCY_BYTE(48000, 1),
	.tSamFreq[0][2] =     UAC_FREQUENCY_BYTE(48000, 2),
};

/* Standard ISO OUT Endpoint Descriptor */
static struct usb_audio_endpoint_descriptor as_out_ep_desc1  = {
    .bLength =            USB_DT_ENDPOINT_AUDIO_SIZE,
    .bDescriptorType =    USB_DT_ENDPOINT,
    .bEndpointAddress =   USB_ISO_OUT_ENDPOINT_ADDRESS,
    .bmAttributes =       USB_ENDPOINT_SYNC_SYNC | USB_ENDPOINT_XFER_ISOC,
    .wMaxPacketSize =     576, //UAC1_ISO_OUT_EP_MAX_PACKET_SIZE,
    .bInterval =          0x01,
	.bRefresh =           0,
	.bSynchAddress =      0,
};
/* Class-specific AS ISO OUT Endpoint Descriptor */
static struct uac_iso_endpoint_descriptor as_iso_out_desc1  = {
    .bLength =            UAC_ISO_ENDPOINT_DESC_SIZE,
    .bDescriptorType =    USB_DT_CS_ENDPOINT,
    .bDescriptorSubtype = UAC_EP_GENERAL,
    .bmAttributes =       1,
    .bLockDelayUnits =    1,
    .wLockDelay =         1,
};


//as ep out alter 2
static struct usb_interface_descriptor as_out_interface_alt_2_desc = {
	.bLength =            USB_DT_INTERFACE_SIZE,
	.bDescriptorType =    USB_DT_INTERFACE,
	.bInterfaceNumber =   AUDIO_STREAM_OUT_INTF_NUM,  // this will be assign automatically
	.bAlternateSetting =  2,
	.bNumEndpoints =      1,
	.bInterfaceClass =    USB_CLASS_AUDIO,
	.bInterfaceSubClass = USB_SUBCLASS_AUDIOSTREAMING,
	.bInterfaceProtocol = 0,
	.iInterface =         0x00,
};

/* B.4.2  Class-Specific AS Interface Descriptor */
static struct uac1_as_header_descriptor as_out_header_desc2 = {
	.bLength =            UAC_DT_AS_HEADER_SIZE,
	.bDescriptorType =    USB_DT_CS_INTERFACE,
	.bDescriptorSubtype = UAC_AS_GENERAL,
	.bTerminalLink =      INPUT_TERMINAL_2CH_ID,
	.bDelay =             1,
	.wFormatTag =         UAC_FORMAT_TYPE_I_PCM,
};

static struct uac_format_type_i_discrete_descriptor as_out_type_i_desc2 = {
	.bLength =            UAC_FORMAT_TYPE_I_DISCRETE_DESC_SIZE(2),
	.bDescriptorType =    USB_DT_CS_INTERFACE,
	.bDescriptorSubtype = UAC_FORMAT_TYPE,
	.bFormatType =        UAC_FORMAT_TYPE_I,
	.bNrChannels =        0x2,
	.bSubframeSize =      3,
	.bBitResolution =     24,
	.bSamFreqType =       1,
	.tSamFreq[0][0] =     UAC_FREQUENCY_BYTE(96000, 0),
	.tSamFreq[0][1] =     UAC_FREQUENCY_BYTE(96000, 1),
	.tSamFreq[0][2] =     UAC_FREQUENCY_BYTE(96000, 2),
};

/* Standard ISO OUT Endpoint Descriptor */
static struct usb_audio_endpoint_descriptor as_out_ep_desc2  = {
	.bLength =            USB_DT_ENDPOINT_AUDIO_SIZE,
	.bDescriptorType =    USB_DT_ENDPOINT,
	.bEndpointAddress =   USB_ISO_OUT_ENDPOINT_ADDRESS,
	.bmAttributes =       USB_ENDPOINT_SYNC_SYNC | USB_ENDPOINT_XFER_ISOC,
	.wMaxPacketSize =     576, //UAC1_ISO_OUT_EP_MAX_PACKET_SIZE,
	.bInterval =          0x01,
	.bRefresh =           0,
	.bSynchAddress =      0,
};

/* Class-specific AS ISO OUT Endpoint Descriptor */
static struct uac_iso_endpoint_descriptor as_iso_out_desc2  = {
	.bLength =            UAC_ISO_ENDPOINT_DESC_SIZE,
	.bDescriptorType =    USB_DT_CS_ENDPOINT,
	.bDescriptorSubtype = UAC_EP_GENERAL,
	.bmAttributes =       1,
	.bLockDelayUnits =    1,
	.wLockDelay =         1,
};


//as ep in
static struct usb_interface_descriptor as_in_interface_alt_0_desc = {
    .bLength =            USB_DT_INTERFACE_SIZE,
    .bDescriptorType =    USB_DT_INTERFACE,
    .bInterfaceNumber =   AUDIO_STREAM_IN_INTF_NUM,
    .bAlternateSetting =  0,
    .bNumEndpoints =      0,
    .bInterfaceClass =    USB_CLASS_AUDIO,
    .bInterfaceSubClass = USB_SUBCLASS_AUDIOSTREAMING,
    .bInterfaceProtocol = 0,
    .iInterface =         0x00,
};
//alt 1 in
static struct usb_interface_descriptor as_in_interface_alt_1_desc = {
    .bLength =            USB_DT_INTERFACE_SIZE,
    .bDescriptorType =    USB_DT_INTERFACE,
    .bInterfaceNumber =   AUDIO_STREAM_IN_INTF_NUM,  // this will be assign automatically
    .bAlternateSetting =  1,
    .bNumEndpoints =      1,
    .bInterfaceClass =    USB_CLASS_AUDIO,
    .bInterfaceSubClass = USB_SUBCLASS_AUDIOSTREAMING,
    .bInterfaceProtocol = 0,
    .iInterface =         0x00,
};

static struct uac1_as_header_descriptor as_in_header_desc1 = {
    .bLength =            sizeof(as_in_header_desc1),
    .bDescriptorType =    USB_DT_CS_INTERFACE,
    .bDescriptorSubtype = UAC_AS_GENERAL,
    .bTerminalLink =      OUTPUT_TERMINAL_USB_ID,
    .bDelay =             0x01,
    .wFormatTag =         UAC_FORMAT_TYPE_I_PCM,
};

static struct  uac_format_type_i_discrete_descriptor as_in_type_i_desc1 = {
    .bLength =            sizeof(as_in_type_i_desc1),
    .bDescriptorType =    USB_DT_CS_INTERFACE,
    .bDescriptorSubtype = UAC_FORMAT_TYPE,
    .bFormatType =        UAC_FORMAT_TYPE_I,
    .bNrChannels =        1,
#if 1
	.bSubframeSize =	  2,
	.bBitResolution =	  16,//
	.bSamFreqType = 	  1,

	.tSamFreq[0][0] =	  UAC_FREQUENCY_BYTE(32000, 0),
	.tSamFreq[0][1] =	  UAC_FREQUENCY_BYTE(32000, 1),
	.tSamFreq[0][2] =	  UAC_FREQUENCY_BYTE(32000, 2),
#else
    .bSubframeSize =      2,
    .bBitResolution =     16,
    .bSamFreqType =       3,

	.tSamFreq[0][0] =     UAC_FREQUENCY_BYTE(16000, 0),
	.tSamFreq[0][1] =     UAC_FREQUENCY_BYTE(16000, 1),
	.tSamFreq[0][2] =     UAC_FREQUENCY_BYTE(16000, 2),

	.tSamFreq[1][0] =     UAC_FREQUENCY_BYTE(32000, 0), //32K
	.tSamFreq[1][1] =     UAC_FREQUENCY_BYTE(32000, 1),
	.tSamFreq[1][2] =     UAC_FREQUENCY_BYTE(32000, 2),

	.tSamFreq[2][0] =     UAC_FREQUENCY_BYTE(48000, 0), //48k
	.tSamFreq[2][1] =     UAC_FREQUENCY_BYTE(48000, 1),
	.tSamFreq[2][2] =     UAC_FREQUENCY_BYTE(48000, 2),
#endif
};

static struct usb_audio_endpoint_descriptor as_in_ep_desc1 = {
    .bLength =            USB_DT_ENDPOINT_AUDIO_SIZE,
    .bDescriptorType =    USB_DT_ENDPOINT,
    .bEndpointAddress =   USB_ISO_IN_ENDPOINT_ADDRESS,
    .bmAttributes =       USB_ENDPOINT_XFER_ISOC | USB_ENDPOINT_SYNC_SYNC,
    .wMaxPacketSize =     512,
    .bInterval =          1,
    .bRefresh =           0,
    .bSynchAddress =      0,
};

static struct uac_iso_endpoint_descriptor as_iso_in_desc1 = {
    .bLength =            USB_DT_ENDPOINT_SIZE,
    .bDescriptorType =    USB_DT_CS_ENDPOINT,
    .bDescriptorSubtype = UAC_EP_GENERAL,
    .bmAttributes =       1,
    .bLockDelayUnits =    0,
    .wLockDelay =         0x00,
};

//alt 2 in
static struct usb_interface_descriptor as_in_interface_alt_2_desc = {
	.bLength =            USB_DT_INTERFACE_SIZE,
	.bDescriptorType =    USB_DT_INTERFACE,
	.bInterfaceNumber =   AUDIO_STREAM_IN_INTF_NUM,  // this will be assign automatically
	.bAlternateSetting =  2,
	.bNumEndpoints =      1,
	.bInterfaceClass =    USB_CLASS_AUDIO,
	.bInterfaceSubClass = USB_SUBCLASS_AUDIOSTREAMING,
	.bInterfaceProtocol = 0,
	.iInterface =         0x00,
};

static struct uac1_as_header_descriptor as_in_header_desc2 = {
	.bLength =            sizeof(as_in_header_desc2),
	.bDescriptorType =    USB_DT_CS_INTERFACE,
	.bDescriptorSubtype = UAC_AS_GENERAL,
	.bTerminalLink =      OUTPUT_TERMINAL_USB_ID,
	.bDelay =             0x01,
	.wFormatTag =         UAC_FORMAT_TYPE_I_PCM,
};

static struct  uac_format_type_i_discrete_descriptor as_in_type_i_desc2 = {
	.bLength =            sizeof(as_in_type_i_desc2),
	.bDescriptorType =    USB_DT_CS_INTERFACE,
	.bDescriptorSubtype = UAC_FORMAT_TYPE,
	.bFormatType =        UAC_FORMAT_TYPE_I,

	.bNrChannels =        1,//1 channel
	.bSubframeSize =      3,
	.bBitResolution =     24,//

	.bSamFreqType =       1,
#if 0
	.tSamFreq[0][0] =     0x80,//48k
	.tSamFreq[0][1] =     0xBB,
	.tSamFreq[0][2] =     0x00,
	
	.tSamFreq[0][0] =     UAC_FREQUENCY_BYTE(32000, 0),
	.tSamFreq[0][1] =     UAC_FREQUENCY_BYTE(32000, 1),
	.tSamFreq[0][2] =     UAC_FREQUENCY_BYTE(32000, 2),

	.tSamFreq[1][0] =     UAC_FREQUENCY_BYTE(32000, 0),
	.tSamFreq[1][1] =     UAC_FREQUENCY_BYTE(32000, 1),
	.tSamFreq[1][2] =     UAC_FREQUENCY_BYTE(32000, 2),
#else
	.tSamFreq[0][0] =     UAC_FREQUENCY_BYTE(48000, 0),
	.tSamFreq[0][1] =     UAC_FREQUENCY_BYTE(48000, 1),
	.tSamFreq[0][2] =     UAC_FREQUENCY_BYTE(48000, 2),
#endif
};
//4.4.2.1 Table 4-17
static struct usb_audio_endpoint_descriptor as_in_ep_desc2 = {
	.bLength =            USB_DT_ENDPOINT_AUDIO_SIZE,
	.bDescriptorType =    USB_DT_ENDPOINT,
	.bEndpointAddress =   USB_ISO_IN_ENDPOINT_ADDRESS,
	.bmAttributes =       USB_ENDPOINT_XFER_ISOC | USB_ENDPOINT_SYNC_SYNC,
	.wMaxPacketSize =     512,
	.bInterval =          1,
	.bRefresh =           0,
	.bSynchAddress =      0,
};

static struct uac_iso_endpoint_descriptor as_iso_in_desc2 = {
	.bLength =            USB_DT_ENDPOINT_SIZE,
	.bDescriptorType =    USB_DT_CS_ENDPOINT,
	.bDescriptorSubtype = UAC_EP_GENERAL,
	.bmAttributes =       1,
	.bLockDelayUnits =    0,
	.wLockDelay =         0x00,
};

static struct usb_descriptor_header *f_audio_desc[]  = {
    (struct usb_descriptor_header *) &ac_interface_desc0,
    (struct usb_descriptor_header *) &ac_header_desc,
    (struct usb_descriptor_header *) &input_terminal_desc0,
    (struct usb_descriptor_header *) &input_terminal_desc1,
    (struct usb_descriptor_header *) &output_terminal_desc0,
    (struct usb_descriptor_header *) &output_terminal_desc1,
    (struct usb_descriptor_header *) &selector_unit_desc,
    (struct usb_descriptor_header *) &feature_unit_desc0,
    (struct usb_descriptor_header *) &feature_unit_desc1,
    (struct usb_descriptor_header *) &feature_unit_desc2,
    (struct usb_descriptor_header *) &mixer_unit_desc,

    (struct usb_descriptor_header *) &as_out_interface_alt_0_desc,
    (struct usb_descriptor_header *) &as_out_interface_alt_1_desc,
    (struct usb_descriptor_header *) &as_out_header_desc1,
    (struct usb_descriptor_header *) &as_out_type_i_desc1,
    (struct usb_descriptor_header *) &as_out_ep_desc1,
    (struct usb_descriptor_header *) &as_iso_out_desc1,

	(struct usb_descriptor_header *) &as_out_interface_alt_2_desc,//alternate 2
	(struct usb_descriptor_header *) &as_out_header_desc2,
	(struct usb_descriptor_header *) &as_out_type_i_desc2,
	(struct usb_descriptor_header *) &as_out_ep_desc2,
	(struct usb_descriptor_header *) &as_iso_out_desc2,

    (struct usb_descriptor_header *) &as_in_interface_alt_0_desc,

    (struct usb_descriptor_header *) &as_in_interface_alt_1_desc,
    (struct usb_descriptor_header *) &as_in_header_desc1,
    (struct usb_descriptor_header *) &as_in_type_i_desc1,
    (struct usb_descriptor_header *) &as_in_ep_desc1,
    (struct usb_descriptor_header *) &as_iso_in_desc1,
#if 1
	(struct usb_descriptor_header *) &as_in_interface_alt_2_desc,//alternate 2
	(struct usb_descriptor_header *) &as_in_header_desc2,
	(struct usb_descriptor_header *) &as_in_type_i_desc2,
	(struct usb_descriptor_header *) &as_in_ep_desc2,
	(struct usb_descriptor_header *) &as_iso_in_desc2,
#endif
    NULL,
};

static void audio_update_audio_info(struct f_audio *audio, unsigned intf, unsigned alt)
{
	struct usb_descriptor_header **phead = f_audio_desc;

	if (AUDIO_STREAM_OUT_INTF_NUM != intf && AUDIO_STREAM_IN_INTF_NUM != intf) {
		return ;
	}
	//USBD_AUDIO_INFO("UAC Intf=%d,alt=%d\n", intf, alt);
	for (; *phead; phead++) {
		//USBD_AUDIO_INFO("UAC (*phead)->bDescriptorType = %d\n", (*phead)->bDescriptorType);
		if( USB_DT_INTERFACE == (*phead)->bDescriptorType) { //find the interfac
			struct usb_interface_descriptor* pinterface = (struct usb_interface_descriptor*)(*phead) ;
			if(pinterface->bInterfaceNumber == intf && pinterface->bAlternateSetting == alt)//find the right pos
			{
				//USBD_AUDIO_INFO("UAC find Intf=%d,alt=%d\n", pinterface->bInterfaceNumber, pinterface->bAlternateSetting);
				for (phead++; *phead; phead++)
				{
					struct uac_format_type_i_discrete_descriptor* psubtype = (struct uac_format_type_i_discrete_descriptor*)(*phead);
					//USBD_AUDIO_INFO("UAC bDescriptorSubtype = %d\n", psubtype->bDescriptorSubtype);
					if(UAC_FORMAT_TYPE == psubtype->bDescriptorSubtype) { //find the format info

						USBD_AUDIO_INFO("UAC set audio format: %d-%d! \n", psubtype->bBitResolution,psubtype->bNrChannels);
						if(AUDIO_STREAM_OUT_INTF_NUM == intf){ //playback
							audio->card.playback.format = (24==psubtype->bBitResolution)?(SOUND_FORMAT_BIT_COUNT_24):(SOUND_FORMAT_BIT_COUNT_16);
							audio->card.playback.channels = psubtype->bNrChannels;
						} else {
							audio->card.capture.format = (24==psubtype->bBitResolution)?(SOUND_FORMAT_BIT_COUNT_24):(SOUND_FORMAT_BIT_COUNT_16);
							audio->card.capture.channels =	psubtype->bNrChannels;
						}
						return ;
					}
				}
			}
		}
	}
}

//audio control
static inline int generic_set_volume_cmd(struct usb_audio_control *con, uint8_t cmd, int value);
static inline int generic_get_volume_cmd(struct usb_audio_control *con, uint8_t cmd);

static inline int generic_set_mute_cmd(struct usb_audio_control *con, uint8_t cmd, int value);
static inline int generic_get_mute_cmd(struct usb_audio_control *con, uint8_t cmd);

//feature 0
static struct usb_audio_control_selector feature_unit_id9 = {
	.id   = FEATURE_UNIT_0_ID,
	.name = "Mute & Volume Control",
	.type = UAC_FEATURE_UNIT,
	.desc = (struct usb_descriptor_header *) &feature_unit_desc0,
};

static struct usb_audio_control mute_control_id9 = {
	.name = "Mute Control",
	.type = UAC_FU_MUTE,
	.set  = generic_set_mute_cmd,
	.get  = generic_get_mute_cmd,
};

static struct usb_audio_control volume_control_id9 = {
	.name = "Volume Control",
	.type = UAC_FU_VOLUME,
	.set  = generic_set_volume_cmd, /* Todo: add real volume control code */
	.get  = generic_get_volume_cmd, /* Todo: add real volume control code */
	.data = {0x0000, 0xF600, 0xDB00, 0x0000, 0x0100},
};

//feature 1
static struct usb_audio_control_selector feature_unit_idA = {
	.id   = FEATURE_UNIT_1_ID,
	.name = "Mute & Volume Control",
	.type = UAC_FEATURE_UNIT,
	.desc = (struct usb_descriptor_header *) &feature_unit_desc1,
};

static struct usb_audio_control mute_control_idA = {
	.name = "Mute Control",
	.type = UAC_FU_MUTE,
	.set  = generic_set_mute_cmd,
	.get  = generic_get_mute_cmd,
};

static struct usb_audio_control volume_control_idA = {
	.name = "Volume Control",
	.type = UAC_FU_VOLUME,
	.set  = generic_set_volume_cmd, /* Todo: add real Mute control code */
	.get  = generic_get_volume_cmd, /* Todo: add real Mute control code */
	.data = {0x0000, 0x0800, 0xF400, 0x0700, 0x0100},
};

//feature 2
static struct usb_audio_control_selector feature_unit_idD = {
	.id = FEATURE_UNIT_2_ID,
	.name = "Mute & Volume Control",
	.type = UAC_FEATURE_UNIT,
	.desc = (struct usb_descriptor_header *) &feature_unit_desc2,
};

static struct usb_audio_control mute_control_idD = {
	.name = "Mute Control",
	.type = UAC_FU_MUTE,
	.set  = generic_set_mute_cmd,
	.get  = generic_get_mute_cmd,
};

static struct usb_audio_control volume_control_idD = {
	.name = "Volume Control",
	.type = UAC_FU_VOLUME,
	.set = generic_set_volume_cmd, /* Todo: add real Mute control code */
	.get = generic_get_volume_cmd, /* Todo: add real Mute control code */
	.data = {0x0000, 0xF900, 0xE900, 0x0800, 0x0100},
};

//mix
static struct usb_audio_control_selector feature_unit_idF = {
	.id = MIXER_UNIT_2_ID,
	.name = "Mute & Volume Control",
	.type = UAC_MIXER_UNIT,
	.desc = (struct usb_descriptor_header *) &mixer_unit_desc,
};

static struct usb_audio_control mute_control_idF = {
	.name = "Mute Control",
	.type = UAC_FU_MUTE,
	.set  = generic_set_mute_cmd,
	.get  = generic_get_mute_cmd,
};

static struct usb_audio_control volume_control_idF = {
	.name = "Volume Control",
	.type = UAC_FU_VOLUME,
	.set = generic_set_volume_cmd, /* Todo: add real Mute control code */
	.get = generic_get_volume_cmd, /* Todo: add real Mute control code */
	.data = {0x0000, 0xF900, 0x0000, 0x0000, 0x0000},   // {0x0000, 0xF900, 0xE900, 0x0800, 0x0100}
};


uint16_t frame_size[2][2][8] = {
	// 16 bit
	{
		{16, 32, 64, 90, 96, 176, 192, 384},      // 1 channel
		{32, 64, 128, 180, 192, 352, 384, 768}    // 2 channel:framesize = 16/8 * 2 * 192K[ICODEC_SR_192K=7]= 768KB/S
	},
	// 24 bit -> represet as 32 bit
	{
		{24, 48, 96, 135, 144, 264, 288, 576},    // 1 channel
		{48, 96, 192, 270, 288, 528, 576, 1152}   // 2 channel:framesize = 24/8 * 2 * 192K[ICODEC_SR_192K=7]= 1152KB/S
	}
};

uint8_t bit_res_table[2] = {2, 3}; // index = 16bit = 2 bytes

struct usb_iso_request *isooutreq = NULL;
struct usb_iso_request *isoinreq = NULL;
uint8_t audio_change_ep_cur = 0xFF;
USB_DSP_STATE usb_dsp_state;
USB_ISOEP_STATE usb_isoep_state;
usbd_audio_usr_cb_t *usb_audio_usr_cb;// usb to app interface
extern uint32_t g_otg_iso_bug_fix;
extern uint8_t g_otg_iso_in_process_buf;

struct f_audio audio_dev;
SRAM_NOCACHE_DATA_SECTION static u8 usb_rx_buf0[1500];
SRAM_NOCACHE_DATA_SECTION static u8 usb_rx_buf1[1500];

static int control_selector_init(struct f_audio *audio);
static void control_selector_deinit(struct f_audio *audio);

/*
	SPEC:uac1.0 5.2.2.2.3 Mixer Control
	value from test 0x8000 ~ 0x7fff
	0x0001 -> + 0.0039(1/256)db
	0x0000 -> 0 db
	0xFFFF -> - 0.0039(1/256)db

	https://www.usbzh.com/article/detail-1127.html
	min 0xd300 ~ max 0x0000
	sound 0x00 ~ 100
*/
//Volume[0~100]  //int
static inline int generic_set_volume_cmd(struct usb_audio_control *con, uint8_t cmd, int oldvalue)
{
	float db = 0 ;
	u16 volume = 0;
	u16 value = (u16)oldvalue ;

	con->data[cmd] = oldvalue;
	value = value & 0xffff;
	if(!(value & 0x8000)) {  // 0db
		value = 0 ;
		db = 0.0;
	} else if(value <= WINDOWS_MIN_VOLUME) {   // -45db
		value = WINDOWS_MIN_VOLUME;
		db = -45.0;
	} else {
		value = 0x10000 - value ;
		db = -1.0 * value / 256 ;
	}
	volume = (u16)(100 * (45.0 + db)/ 45.0 );

	//USBD_AUDIO_INFO("Run generic_set_volume_cmd,cmd=%d,value=%d/%d[%f-%f]\n", cmd, oldvalue, value, db, volume);
	if (usb_audio_usr_cb && usb_audio_usr_cb->set_volume) {
		usb_audio_usr_cb->set_volume(volume);
	}

	return 0;
}
static inline int generic_get_volume_cmd(struct usb_audio_control *con, uint8_t cmd)
{
	USBD_AUDIO_INFO("Run generic_get_volume_cmd, cmd=%d\n", cmd);
	return con->data[cmd];
}
static inline int generic_set_mute_cmd(struct usb_audio_control *con, uint8_t cmd, int value)
{
	UNUSED(con);
	UNUSED(cmd);
	USBD_AUDIO_INFO("Run generic_set_mute_cmd,cmd=%d,value=%d\n", cmd, value);
	//value  =1, mute
	//value !=1, unmute
	con->data[cmd] = value;

	if (usb_audio_usr_cb && usb_audio_usr_cb->set_mute) {
		usb_audio_usr_cb->set_mute(value);
	} 
	return 0;
}
static inline int generic_get_mute_cmd(struct usb_audio_control *con, uint8_t cmd)
{
	USBD_AUDIO_INFO("Run generic_get_mute_cmd, cmd=%d\n", cmd);
	return con->data[cmd];
}

static void process_iso_out_buffer(struct usb_ep *ep, struct usb_iso_request *isoreq)
{
	//uint32_t real_ep_len;
	uint32_t real_len = 0;
	uint32_t *addr = NULL;
	UNUSED(ep);
	//real_ep_len = (UAC1_ISO_OUT_EP_MAX_PACKET_SIZE - g_otg_iso_bug_fix);

	if (isoreq->iso_packet_desc0->actual_length != 0) {
		addr = (u32 *)isoreq->buf0;
		real_len = isoreq->iso_packet_desc0->actual_length;
		usb_audio_usr_cb->put_data((uint8_t *)addr, real_len);
		isoreq->iso_packet_desc0->actual_length = 0;
	}

	if (isoreq->iso_packet_desc1->actual_length != 0) {
		addr = (uint32_t *)isoreq->buf1;
		real_len = isoreq->iso_packet_desc1->actual_length;
		usb_audio_usr_cb->put_data((uint8_t *)addr, real_len);
		isoreq->iso_packet_desc1->actual_length = 0;
	}
}

static void process_iso_in_buffer(struct usb_ep *ep, struct usb_iso_request *isoreq)
{
    uint8_t tmp;
    uint32_t tempLen = isoinreq->data_per_frame;
    
    UNUSED(ep);

    if (g_otg_iso_in_process_buf == 0) {
        tmp = usb_audio_usr_cb->pull_data((uint8_t *)isoreq->buf1, tempLen);
        if (tmp == 0) {
            rtw_memset((uint8_t *)isoreq->buf1, 0, tempLen);
        }
    } else {
        tmp = usb_audio_usr_cb->pull_data((uint8_t *)isoreq->buf0, tempLen);
        if (tmp == 0) {
            rtw_memset((uint8_t *)isoreq->buf0, 0, tempLen);
        }
    }
}

static int audio_out_ep_complete(struct usb_ep *ep, struct usb_request *req)
{
    UNUSED(ep);
    UNUSED(req);
    return 0;
}

static void audio_complete(struct usb_ep *ep, struct usb_request *req)
{
    struct f_audio *audio = req->context;
    int status = req->status;
    uint32_t data = 0;
    struct usb_ep *out_ep = audio->out_ep;

    switch (status) {
        case 0:                         /* normal completion? */
            if (ep == out_ep) {
                audio_out_ep_complete(ep, req);
            } else if (audio->set_con) {
                rtw_memcpy(&data, req->buf, req->length);
                audio->set_con->set(audio->set_con, audio->set_cmd,
                    //le16_to_cpu(data));
                    (data));
                audio->set_con = NULL;
            } else if (audio_change_ep_cur == USB_ISO_OUT_ENDPOINT_ADDRESS) {
                uint32_t new_samp_freq = (*((uint32_t *)req->buf)) & 0xFFFFFF;
                int new_sample_freq_idx = audio->card.playback.sample_freq_idx;

                for (uint8_t i = 0; i < 8; i++) {
                    if (new_samp_freq == pcm_rate_table[i]) {
                        new_sample_freq_idx = i;
                    }
                }

                if (new_sample_freq_idx != audio->card.playback.sample_freq_idx) {
                    audio->card.playback.sample_freq_idx = new_sample_freq_idx;
                    audio_change_ep_cur = 0xFF;

                    if ((isooutreq != NULL) && (usb_dsp_state.usb_da_state ==  DSP_START)) {
                        // stop
                        usb_audio_usr_cb->unplug(IDX_DA);
                        usb_dsp_state.usb_da_state =  DSP_STOP;
                        iso_ep_stop(audio->out_ep, isooutreq);
                        // start
                        uint8_t ch_cnt = audio->card.playback.channels - 1;
                        isooutreq->data_per_frame =
                            frame_size[audio->card.playback.format][ch_cnt][audio->card.playback.sample_freq_idx];
                        iso_ep_start(audio->out_ep, isooutreq, 1);
                        usb_audio_usr_cb->plug(IDX_DA, audio->card.playback.format,
                            audio->card.playback.sample_freq_idx, audio->card.playback.channels);
                        usb_dsp_state.usb_da_state =  DSP_START;
                    }
                }
            } else if ((audio_change_ep_cur == USB_ISO_IN_ENDPOINT_ADDRESS)) {
                // handle
                uint32_t new_samp_freq = (*((uint32_t *)req->buf)) & 0xFFFFFF;
                int new_sample_freq_idx = audio->card.capture.sample_freq_idx;

                for (uint8_t i = 0; i < 8; i++) {
                    if (new_samp_freq == pcm_rate_table[i]) {
                        new_sample_freq_idx = i;
                    }
                }

                if (new_sample_freq_idx != audio->card.capture.sample_freq_idx) {
                    audio->card.capture.sample_freq_idx = new_sample_freq_idx;
                    audio_change_ep_cur = 0xFF;

                    if ((isoinreq != NULL) && (usb_dsp_state.usb_ad_state ==  DSP_START)) {
                        // stop
                        usb_audio_usr_cb->unplug(IDX_AD);
                        usb_dsp_state.usb_ad_state =  DSP_STOP;
                        iso_ep_stop(audio->in_ep, isoinreq);
                        // start
                        uint8_t ch_cnt = audio->card.capture.channels - 1;
                        isoinreq->data_per_frame //= DATA_PER_FRAME_TMP; //= 192;   //= 96;
                            = frame_size[audio->card.capture.format][ch_cnt][audio->card.capture.sample_freq_idx];
                        iso_ep_start(audio->in_ep, isoinreq, 1);
                        usb_audio_usr_cb->plug(IDX_AD, audio->card.capture.format,
                            audio->card.capture.sample_freq_idx, audio->card.capture.channels);
                        usb_dsp_state.usb_ad_state =  DSP_START;
                    }
                }
            }

            break;

        default:
            break;
    }
}

static int audio_set_intf_req(struct usb_function *f, const struct usb_control_request *ctrl)
{
    struct f_audio *audio = &audio_dev;
    struct usb_composite_dev *cdev = f->config->cdev;
    struct usb_request *req = cdev->req;
    uint8_t id = (((ctrl->wIndex) >> 8) & 0xFF);
    uint16_t len = (ctrl->wLength);
    uint16_t w_value = (ctrl->wValue);
    uint8_t con_sel = (w_value >> 8) & 0xFF;
    uint8_t cmd = (ctrl->bRequest & 0x0F);
    struct usb_audio_control_selector *cs;//dwc_list_link
    struct usb_audio_control *con;
    
    dwc_list_link_t *cs_list = NULL;
    dwc_list_link_t *con_list = NULL;
    
    DWC_LIST_FOREACH(cs_list, &audio->cs) {
        cs = DWC_LIST_ENTRY(cs_list, struct usb_audio_control_selector, list);
        if (cs->id == id) {
            DWC_LIST_FOREACH(con_list, &cs->control) {
                con = DWC_LIST_ENTRY(con_list, struct usb_audio_control, list);
                if (con->type == con_sel) {
                    audio->set_con = con;
                    break;
                }
            }
        }
    }
    
    audio->set_cmd = cmd;
    req->context = audio;
    req->complete = audio_complete;
    
    return len;
}

static int audio_get_intf_req(struct usb_function *f, const struct usb_control_request *ctrl)
{
    struct usb_composite_dev *cdev = f->config->cdev;
    struct usb_request *req = cdev->req;
    struct f_audio *audio = &audio_dev;
    int value = -USB_EOPNOTSUPP;
    uint8_t id = (((ctrl->wIndex) >> 8) & 0xFF);
    uint16_t len = (ctrl->wLength);
    uint16_t w_value = (ctrl->wValue);
    uint8_t con_sel = (w_value >> 8) & 0xFF;
    uint8_t cmd = (ctrl->bRequest & 0x0F);
    struct usb_audio_control_selector *cs;
    struct usb_audio_control *con;
    
    dwc_list_link_t *cs_list = NULL;
    dwc_list_link_t *con_list = NULL;
    
    DWC_LIST_FOREACH(cs_list, &audio->cs) {
        cs = DWC_LIST_ENTRY(cs_list, struct usb_audio_control_selector, list);
        if (cs->id == id) {
            DWC_LIST_FOREACH(con_list, &cs->control) {
                con = DWC_LIST_ENTRY(con_list, struct usb_audio_control, list);
                if (con->type == con_sel && con->get) {
                    value = con->get(con, cmd);
                    break;
                }
            }
        }
    }
    
    req->context = audio;
    req->complete = audio_complete;
    
    len = MIN(sizeof(value), len);
    rtw_memcpy(req->buf, &value, len);
    
    return len;
}

static int audio_set_endpoint_req(struct usb_function *f, const struct usb_control_request *ctrl)
{
    struct usb_composite_dev *cdev = f->config->cdev;
    struct f_audio *audio = &audio_dev;
    int value = -USB_EOPNOTSUPP;
    uint16_t ep = (ctrl->wIndex);
    uint16_t len = (ctrl->wLength);
    struct usb_request *req = cdev->req;

    switch (ctrl->bRequest) {
        case UAC_SET_CUR:
            value = len;

            if (ep == USB_ISO_IN_ENDPOINT_ADDRESS) {
                audio_change_ep_cur = USB_ISO_IN_ENDPOINT_ADDRESS;
            } else if (ep == USB_ISO_OUT_ENDPOINT_ADDRESS) {
                audio_change_ep_cur = USB_ISO_OUT_ENDPOINT_ADDRESS;
            }

            break;

        case UAC_SET_MIN:
            break;

        case UAC_SET_MAX:
            break;

        case UAC_SET_RES:
            break;

        case UAC_SET_MEM:
            break;

        default:
            break;
    }

    // bbpro add
    req->context = audio;
    req->complete = audio_complete;
    return value;
}

static int audio_get_endpoint_req(struct usb_function *f, const struct usb_control_request *ctrl)
{
    int value = -USB_EOPNOTSUPP;
    uint16_t len = (ctrl->wLength);
    UNUSED(f);

    switch (ctrl->bRequest) {
        case UAC_GET_CUR:
        case UAC_GET_MIN:
        case UAC_GET_MAX:
        case UAC_GET_RES:
            value = len;
            break;

        case UAC_GET_MEM:
            break;

        default:
            break;
    }

    return value;
}

static int audio_fun_setup(struct usb_function *f, const struct usb_control_request *ctrl)
{
    struct usb_composite_dev *cdev = f->config->cdev;
    struct usb_request      *req = cdev->req;
    int                     value = -USB_EOPNOTSUPP;

	USBD_AUDIO_ENTER;
	USBD_AUDIO_INFO("[UAC] bmRequestType=0x%02X bRequest=0x%02X wValue=0x%04X wIndex=0x%04X wLength=0x%02X\n",
					ctrl->bmRequestType,
					ctrl->bRequest,
					ctrl->wValue,
					ctrl->wIndex,
					ctrl->wLength);

    /* composite driver infrastructure handles everything; interface
     * activation uses set_alt().
     */
    switch (ctrl->bmRequestType) {
        case USB_DIR_OUT | USB_TYPE_CLASS | USB_RECIP_INTERFACE:
            value = audio_set_intf_req(f, ctrl);
            break;

        case USB_DIR_IN | USB_TYPE_CLASS | USB_RECIP_INTERFACE:
            value = audio_get_intf_req(f, ctrl);
            break;

        case USB_DIR_OUT | USB_TYPE_CLASS | USB_RECIP_ENDPOINT:
            value = audio_set_endpoint_req(f, ctrl);
            break;

        case USB_DIR_IN | USB_TYPE_CLASS | USB_RECIP_ENDPOINT:
            value = audio_get_endpoint_req(f, ctrl);
            break;

        default:
            USBD_AUDIO_ERROR("invalid control req%02x.%02x v%04x i%04x l%d",
                ctrl->bmRequestType, ctrl->bRequest,
                ctrl->wValue, ctrl->wIndex, ctrl->wLength);
    }

    /* respond with data transfer or status phase? */
    if (value >= 0) {
        req->zero = 0;
        req->length = value;
        value = usb_ep_queue(cdev->gadget->ep0, req, 1);
        if (value < 0) {
            USBD_AUDIO_ERROR("audio response on err %d\n", value);
        }
    }

    USBD_AUDIO_EXIT;
    
    /* device either stalls (value < 0) or reports success */
    return value;
}

static int down_isoin(struct f_audio *audio)
{
    int err = 0;

    if (usb_dsp_state.usb_ad_state == DSP_START) {
        usb_audio_usr_cb->unplug(IDX_AD);
        usb_dsp_state.usb_ad_state = DSP_STOP;
        err = iso_ep_stop(audio->in_ep, isoinreq);
    }

    return err;
}

static int up_isoin(struct f_audio *audio)
{
    int err = 0;
    struct usb_ep *in_ep = audio->in_ep;

    if ((isoinreq != NULL) && (usb_dsp_state.usb_ad_state == DSP_STOP)) {
        if (usb_isoep_state.usb_INEP_state == ISOEP_DISABLE) {
            isoinreq->process_buffer = process_iso_in_buffer;
            isoinreq->context = audio;
            isoinreq->buf_proc_intrvl = 1;
            isoinreq->sync_frame = 0;   // useless?
            isoinreq->flags = 1;
            isoinreq->start_frame = 0;
            isoinreq->dma0 = (dma_addr_t)isoinreq->buf0;
            isoinreq->dma1 = (dma_addr_t)isoinreq->buf1;
            in_ep->desc = (struct usb_endpoint_descriptor *)&as_in_ep_desc1;
            err = usb_ep_enable(in_ep, in_ep->desc);
            usb_isoep_state.usb_INEP_state = ISOEP_ENABLE;

            if (err != 0) {
                USBD_AUDIO_ERROR("enable ISO IN EP fail");
            }
        }

        //2 enable dsp
        uint8_t ch_cnt = audio->card.capture.channels - 1;
        isoinreq->data_per_frame = frame_size[audio->card.capture.format][ch_cnt][audio->card.capture.sample_freq_idx];
        usb_audio_usr_cb->plug(IDX_AD, audio->card.capture.format,
            audio->card.capture.sample_freq_idx, audio->card.capture.channels);
        iso_ep_start(audio->in_ep, isoinreq, 1);
        usb_dsp_state.usb_ad_state = DSP_START;
    }

    return err;
}

static int down_isoout(struct f_audio *audio)
{
    int err = 0;

    if (usb_dsp_state.usb_da_state == DSP_START) {
        usb_audio_usr_cb->unplug(IDX_DA);
        usb_dsp_state.usb_da_state = DSP_STOP;
        err = iso_ep_stop(audio->out_ep, isooutreq);
    }

    return err;
}

static int up_isoout(struct f_audio *audio)
{
    int err = 0;
    struct usb_ep *out_ep = audio->out_ep;

    if ((isooutreq != NULL) && (usb_dsp_state.usb_da_state == DSP_STOP)) {
        if (usb_isoep_state.usb_OUTEP_state == ISOEP_DISABLE) {
            isooutreq->process_buffer = process_iso_out_buffer;//complete_cb
            isooutreq->context = audio;
            isooutreq->buf_proc_intrvl = 1;//1;
            isooutreq->sync_frame = 0;// useless?
            isooutreq->flags = 1;
            isooutreq->start_frame = 0;
            isooutreq->dma0 = (dma_addr_t)isooutreq->buf0;
            isooutreq->dma1 = (dma_addr_t)isooutreq->buf1;
            out_ep->desc = (struct usb_endpoint_descriptor *)&as_out_ep_desc1;
            err = usb_ep_enable(out_ep, out_ep->desc);
            usb_isoep_state.usb_OUTEP_state = ISOEP_ENABLE;

            if (err != 0) {
                USBD_AUDIO_ERROR("enable ISO OUT EP fail");
            }
        }

        //2 enable dsp
        uint8_t ch_cnt = audio->card.playback.channels - 1;
        isooutreq->data_per_frame = frame_size[audio->card.playback.format][ch_cnt][audio->card.playback.sample_freq_idx];
        usb_audio_usr_cb->plug(IDX_DA, audio->card.playback.format,
            audio->card.playback.sample_freq_idx, audio->card.playback.channels);
        usb_dsp_state.usb_da_state = DSP_START;
        iso_ep_start(audio->out_ep, isooutreq, 1);
    }

    return err;
}

/**
* enable endpoints and allocate usb request
* this cb will be called when valid set_configuration, and init all iinterface's alt = 0
*/
static int audio_fun_set_alt(struct usb_function *function, unsigned intf, unsigned alt)
{
    struct f_audio  *audio = &audio_dev;
    int err = 0;

    UNUSED(function);
    
    USBD_AUDIO_INFO("UAC Intf=%d,alt=%d\n", intf, alt);

    //1 TODO: should be init ac_alt, as_alt, ac_intf, ac_intf = 0
    if (AUDIO_CONTROL_INTF_NUM == intf) {
        audio->ac_intf = AUDIO_CONTROL_INTF_NUM;
        audio->ac_alt = alt; // audio control alt must 0 for bbpro
    } else if (AUDIO_STREAM_OUT_INTF_NUM == intf) {
        audio->as_out_intf = AUDIO_STREAM_OUT_INTF_NUM;
        audio->as_out_alt = alt;
    } else if (AUDIO_STREAM_IN_INTF_NUM == intf) {
        audio->as_in_intf = AUDIO_STREAM_IN_INTF_NUM;
        audio->as_in_alt = alt;
    }

	if (intf == AUDIO_STREAM_OUT_INTF_NUM) {//alt index : 0 1 2
		if (alt == 0) {
			if (isooutreq != NULL) {
				err = down_isoout(audio);//no data trans
			}
		} else if (alt == 1) { //same as that defined in as_interface
			audio_update_audio_info(audio,intf,alt);
			err = up_isoout(audio);
		} else if (alt == 2) {
			audio_update_audio_info(audio,intf,alt);
			err = up_isoout(audio);
		} else {
			err = -1;
		}
    }
    else if (intf == AUDIO_STREAM_IN_INTF_NUM) {
        if (alt == 0) {
            if (isoinreq != NULL) {
                down_isoin(audio);
            }
        } else if (alt == 1) {
	        audio_update_audio_info(audio,intf,alt);
            err = up_isoin(audio);
		} else if (alt == 2) {
			audio_update_audio_info(audio,intf,alt);
			err = up_isoin(audio);
        } else {
            err = -USB_ENOMEM;
        }
    }

    USBD_AUDIO_EXIT;

    return err;
}

static int audio_fun_get_alt(struct usb_function *function, unsigned intf)
{
    struct f_audio *audio = &audio_dev;
    uint8_t alt;
    
    UNUSED(function);
    
    USBD_AUDIO_ENTER;

    // bbpro add
    if (intf == audio->ac_intf) {
        alt = audio->ac_alt;
    } else if (intf == audio->as_out_intf) {
        alt = audio->as_out_alt;
    } else  if (intf == audio->as_in_intf) {
        alt = audio->as_in_alt;
    } else {
        alt = 0;
    }

    USBD_AUDIO_EXIT;

    return alt;
}

static int audio_fun_bind(struct usb_configuration *c, struct usb_function *function)
{
    struct usb_ep *ep;
    int status = -USB_ENODEV;
    int id;
    struct f_audio  *audio = &audio_dev;

    UNUSED(function);
    
    USBD_AUDIO_ENTER;
    
    audio->card.gadget = c->cdev->gadget;
    
    /* allocate instance-specific interface IDs, and patch descriptors */
    id = usb_interface_id(c, function); // this will return the allocated interface id
    if (id < 0) {
        status = id;
        goto audio_fun_bind_fail;
    }

    ac_interface_desc0.bInterfaceNumber = id;
    id = usb_interface_id(c, function); // this will return the allocated interface id
    if (id < 0) {
        status = id;
        goto audio_fun_bind_fail;
    }

    as_out_interface_alt_0_desc.bInterfaceNumber = id;
    as_out_interface_alt_1_desc.bInterfaceNumber = id;
	as_out_interface_alt_2_desc.bInterfaceNumber = id;
    
    /* search endpoint according to endpoint descriptor and modify endpoint address*/
    ep = usb_ep_autoconfig(c->cdev->gadget, (struct usb_endpoint_descriptor *)&as_out_ep_desc1);
    if (!ep) {
        goto audio_fun_bind_fail;
    }

    ep->driver_data = audio; /* claim */
    audio->out_ep = ep;
    
    id = usb_interface_id(c, function); // this will return the allocated interface id
    if (id < 0) {
        status = id;
        goto audio_fun_bind_fail;
    }

    as_in_interface_alt_0_desc.bInterfaceNumber = id;
    as_in_interface_alt_1_desc.bInterfaceNumber = id;
	as_in_interface_alt_2_desc.bInterfaceNumber = id;
    
    /* search endpoint according to endpoint descriptor and modify endpoint address*/
    ep = usb_ep_autoconfig(c->cdev->gadget, (struct usb_endpoint_descriptor *)&as_in_ep_desc1);
    if (!ep) {
        goto audio_fun_bind_fail;
    }

    ep->driver_data = audio; /* claim */
    audio->in_ep = ep;
    
    status = usb_assign_descriptors(function, (struct usb_descriptor_header **)f_audio_desc, NULL, NULL);
    if (status) {
        goto audio_fun_bind_fail;
    }
    
    isoinreq = alloc_iso_request(audio->in_ep, 1, 1);
    isoinreq->buf0 =  rtw_malloc(g_req_in_buf_size);
    isoinreq->buf1 = rtw_malloc(g_req_in_buf_size);
    if ((isoinreq == NULL) || (isoinreq->buf0 == NULL) || (isoinreq->buf1 == NULL)) {
        USBD_AUDIO_ERROR("isoinreq allocate fail");
        status = -USB_ENOMEM;
        goto audio_fun_bind_fail;
    }

    isooutreq = alloc_iso_request(audio->out_ep, 1, 1);
    isooutreq->buf0 = usb_rx_buf0;
    isooutreq->buf1 = usb_rx_buf1;
    if ((isooutreq == NULL) || (isooutreq->buf0 == NULL) || (isooutreq->buf1 == NULL)) {
        USBD_AUDIO_ERROR("isooutreq allocate fail");
        rtw_free(isoinreq->buf0);
        isoinreq->buf0 = NULL;
        rtw_free(isoinreq->buf1);
        isoinreq->buf1 = NULL;
        free_iso_request(audio->in_ep, isoinreq);
        isoinreq = NULL;
        status = -USB_ENOMEM;
        goto audio_fun_bind_fail;
    }

    usb_isoep_state.usb_INEP_state = ISOEP_DISABLE;
    usb_isoep_state.usb_OUTEP_state = ISOEP_DISABLE;
    usb_dsp_state.usb_ad_state = DSP_STOP;
    usb_dsp_state.usb_da_state = DSP_STOP;

    USBD_AUDIO_EXIT;

    return 0;

audio_fun_bind_fail:
    
    usb_free_all_descriptors(function);
    
    USBD_AUDIO_EXIT_ERR;
    
    return status;
}

static void audio_fun_unbind(struct usb_configuration *c, struct usb_function *function)
{
    struct f_audio  *audio = &audio_dev;
    
    UNUSED(c);
    UNUSED(function);
    
    USBD_AUDIO_ENTER;

    usbd_audio_stop();
    
    control_selector_deinit(audio);
    
    USBD_AUDIO_EXIT;
}

static void audio_fun_disable(struct usb_function *function)
{
    struct f_audio  *audio = &audio_dev;

    UNUSED(function);
    
    USBD_AUDIO_ENTER;
    
    usb_ep_disable(audio->out_ep);
    audio->out_ep->driver_data = NULL;
    // disable iso in EP
    usb_ep_disable(audio->in_ep);
    audio->in_ep->driver_data = NULL;
    
    USBD_AUDIO_EXIT;
}

static void audio_fun_free(struct usb_function *function)
{
    struct f_audio  *audio = &audio_dev;
    
    USBD_AUDIO_ENTER;
    
    if (isoinreq != NULL) {
        if (isoinreq->buf0 != NULL) {
            rtw_free(isoinreq->buf0);
            isoinreq->buf0 = NULL;
        }
        if (isoinreq->buf1 != NULL) {
            rtw_free(isoinreq->buf1);
            isoinreq->buf1 = NULL;
        }
        free_iso_request(audio->in_ep, isoinreq);
        isoinreq = NULL;
    }
    
    if (isooutreq != NULL) {
        free_iso_request(audio->out_ep, isooutreq);
        isooutreq = NULL;
    }
    
    if (pcm_rate_table != NULL) {
        rtw_free(pcm_rate_table);
        pcm_rate_table = NULL;
    }
    
    usb_free_all_descriptors(function);
    
    USBD_AUDIO_EXIT;
}

/* Todo: add more control selecotor dynamically */
static int control_selector_init(struct f_audio *audio)
{
    DWC_LIST_INIT(&audio->cs);
    
    //3 ID 9
    DWC_LIST_INIT(&feature_unit_id9.list);
    DWC_LIST_INIT(&mute_control_id9.list);
    DWC_LIST_INIT(&volume_control_id9.list);
    DWC_LIST_INSERT_TAIL(&audio->cs, &feature_unit_id9.list);
    DWC_LIST_INIT(&feature_unit_id9.control);
    DWC_LIST_INSERT_TAIL(&feature_unit_id9.control, &mute_control_id9.list);
    DWC_LIST_INSERT_TAIL(&feature_unit_id9.control, &volume_control_id9.list);
    
    //3 ID A
    DWC_LIST_INIT(&feature_unit_idA.list);
    DWC_LIST_INIT(&volume_control_idA.list);
    DWC_LIST_INIT(&mute_control_idA.list);
    DWC_LIST_INSERT_TAIL(&audio->cs, &feature_unit_idA.list);
    DWC_LIST_INIT(&feature_unit_idA.control);
    DWC_LIST_INSERT_TAIL(&feature_unit_idA.control, &mute_control_idA.list);
    DWC_LIST_INSERT_TAIL(&feature_unit_idA.control, &volume_control_idA.list);
    
    //3 ID D
    DWC_LIST_INIT(&feature_unit_idD.list);
    DWC_LIST_INIT(&mute_control_idD.list);
    DWC_LIST_INIT(&volume_control_idD.list);
    DWC_LIST_INSERT_TAIL(&audio->cs, &feature_unit_idD.list);
    DWC_LIST_INIT(&feature_unit_idD.control);
    DWC_LIST_INSERT_TAIL(&feature_unit_idD.control, &mute_control_idD.list);
    DWC_LIST_INSERT_TAIL(&feature_unit_idD.control, &volume_control_idD.list);

    //3 ID F
    DWC_LIST_INIT(&feature_unit_idF.list);
    DWC_LIST_INIT(&mute_control_idF.list);
    DWC_LIST_INIT(&volume_control_idF.list);
    DWC_LIST_INSERT_TAIL(&audio->cs, &feature_unit_idF.list);
    DWC_LIST_INIT(&feature_unit_idF.control);
    DWC_LIST_INSERT_TAIL(&feature_unit_idF.control, &mute_control_idF.list);
    DWC_LIST_INSERT_TAIL(&feature_unit_idF.control, &volume_control_idF.list);
    
    return 0;
}

static void control_selector_deinit(struct f_audio *audio)
{
    DWC_LIST_INIT(&audio->cs);
    
    //3 ID 9
    DWC_LIST_INIT(&feature_unit_id9.list);
    DWC_LIST_INIT(&mute_control_id9.list);
    DWC_LIST_INIT(&volume_control_id9.list);
    DWC_LIST_INIT(&feature_unit_id9.control);
    
    //3 ID A
    DWC_LIST_INIT(&feature_unit_idA.list);
    DWC_LIST_INIT(&volume_control_idA.list);
    DWC_LIST_INIT(&mute_control_idA.list);
    DWC_LIST_INIT(&feature_unit_idA.control);
    
    //3 ID D
    DWC_LIST_INIT(&feature_unit_idD.list);
    DWC_LIST_INIT(&mute_control_idD.list);
    DWC_LIST_INIT(&volume_control_idD.list);
    DWC_LIST_INIT(&feature_unit_idD.control);

    //3 ID F
    DWC_LIST_INIT(&feature_unit_idF.list);
    DWC_LIST_INIT(&mute_control_idF.list);
    DWC_LIST_INIT(&volume_control_idF.list);
    DWC_LIST_INIT(&feature_unit_idF.control);
}

static int audio_config(struct usb_configuration *c)
{
    int status = 0;
    struct f_audio *audio = &audio_dev;
    
    USBD_AUDIO_ENTER;
    
    audio->gadget = c->cdev->gadget;
    
    control_selector_init(audio);
    
    audio->card.playback.sample_freq_idx = ICODEC_SR_48K;
    audio->card.playback.format = SOUND_FORMAT_BIT_COUNT_16;
    audio->card.playback.channels = DEFAULT_PLAY_CH;
	
    audio->card.capture.sample_freq_idx = ICODEC_SR_32K;
    audio->card.capture.format = SOUND_FORMAT_BIT_COUNT_16;
    audio->card.capture.channels =  DEFAULT_RECORD_CH;

    // pre allocate request
    pcm_rate_table = (uint32_t *)rtw_malloc(PCM_TABLE_CNT * 4);
    if (pcm_rate_table == NULL) {
        USBD_AUDIO_ERROR("PCM rate table allocate fail");
        status = -USB_ENOMEM;
        goto audio_config_fail;
    } else {
        *(pcm_rate_table + 0) = 8000;
        *(pcm_rate_table + 1) = 16000;
        *(pcm_rate_table + 2) = 32000;
        *(pcm_rate_table + 3) = 44100;
        *(pcm_rate_table + 4) = 48000;
        *(pcm_rate_table + 5) = 88000;
        *(pcm_rate_table + 6) = 96000;
        *(pcm_rate_table + 7) = 192000;
    }

    USBD_AUDIO_EXIT;
    return USB_ESUCCESS;

audio_config_fail:
    USBD_AUDIO_EXIT_ERR;
    return status;
}

static struct usb_function audio_funcions = {
    .bind      = audio_fun_bind,
    .unbind    = audio_fun_unbind,
    .setup     = audio_fun_setup,
    .set_alt   = audio_fun_set_alt,
    .get_alt   = audio_fun_get_alt,
    .disable   = audio_fun_disable,
    .free_func = audio_fun_free,
};

static struct usb_composite_driver audio_driver = {
	.dev_desc	 = &audio_device_desc,
	.strings 	 = audio_gadget_strings,
	.config_desc = &audio_config_desc,
	.functions   = &audio_funcions,
	.driver_data = &audio_dev,
	.config      = audio_config,
};

int usbd_audio_init(usbd_audio_usr_cb_t *cb)
{
    int status = 0;
    
    USBD_AUDIO_ENTER;

    g_req_out_buf_size = MAX_REQ_SIZE;
    g_req_in_buf_size = MAX_REQ_SIZE;

    usb_isoep_state.usb_INEP_state = ISOEP_DISABLE;
    usb_isoep_state.usb_OUTEP_state = ISOEP_DISABLE;
    usb_dsp_state.usb_ad_state = DSP_STOP;
    usb_dsp_state.usb_da_state = DSP_STOP;

    if (cb != NULL) {
        usb_audio_usr_cb = cb;
    }

    status = usb_composite_register(&audio_driver);
    if (status != 0) {
        USBD_AUDIO_ERROR("Register composite driver fail");
        goto usbd_audio_init_fail;
    }
    
    if (usb_audio_usr_cb->init != NULL) {
        status = usb_audio_usr_cb->init();
        if (status != USB_ESUCCESS) {
            USBD_AUDIO_ERROR("User init fail");
            if (usb_audio_usr_cb->deinit != NULL) {
                usb_audio_usr_cb->deinit();
            }
            goto usbd_audio_init_fail;
        }
    }
    
    USBD_AUDIO_EXIT;
    return USB_ESUCCESS;
    
usbd_audio_init_fail:
    USBD_AUDIO_EXIT_ERR;
    usb_composite_unregister(&audio_driver);
    return status;
}

void usbd_audio_deinit(void)
{
    USBD_AUDIO_ENTER;
    
    if (usb_audio_usr_cb->deinit != NULL) {
        usb_audio_usr_cb->deinit();
    }
    
	usb_composite_unregister(&audio_driver);
    
    USBD_AUDIO_EXIT;
}

void usbd_audio_stop(void)
{
    if (usb_audio_usr_cb->unplug) {
        usb_audio_usr_cb->unplug(IDX_DA);
        usb_audio_usr_cb->unplug(IDX_AD);
    }
    
    usb_dsp_state.usb_ad_state = DSP_STOP;
    usb_dsp_state.usb_da_state =  DSP_STOP;
    usb_isoep_state.usb_OUTEP_state = ISOEP_DISABLE;
    usb_isoep_state.usb_INEP_state = ISOEP_DISABLE;
}

#endif // CONFIG_USBD_AUDIO

