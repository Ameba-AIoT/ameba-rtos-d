RTSP Client Service 
Setup Guide
        1. Add rtsp client example to SDK
            [GCC]
                (i)  Uncomment below line in GCC-RELEASE\project_hp\asdk\make\utilities_example\Makefile to build the example

                    CSRC += $(DIR)/rtsp_client/example_rtsp_client.c

                (ii) Uncomment "make -C g711 all" in GCC-RELEASE\project_hp\asdk\make\audio\Makefile,

                    "CSRC += $(DIR)/rtsp_client_api.c" in GCC-RELEASE\project_hp\asdk\make\network\rtsp\Makefile

                (iii) Make sure lib_rtsp.a, lib_rtp_codec.a and lib_g711.a are linked in GCC-RELEASE\project_hp\asdk\Makefile

                    LINK_APP_LIB += $(ROOTDIR)/lib/application/lib_rtsp.a
                    LINK_APP_LIB += $(ROOTDIR)/lib/application/lib_rtp_codec.a
                    LINK_APP_LIB += $(ROOTDIR)/lib/application/lib_g711.a

       2. Enable CONFIG_EXAMPLE_RTP_SERVER in [platform_opts.h]

            /* For rtp server example */
            #define CONFIG_EXAMPLE_RTSP_CLIENT               1

       3. Use libc to parse the string of session id as an unsigned number

Usage
        1.RTSP Client Service starts after calling the entry function example_rtsp_client()
	    in sdk\component\common\example\example_entry.c

        2. Set the server address and stream name in sdk\component\common\example\rtsp_client\example_rtsp_client.c
	#define REMOTE_IP 	"54.156.189.88"
	#define REMOTE_PORT 	554
	#define STREAM_NAME	"mediaStreaming/abcde"

        3. Set the codec id, sample rate and cast mode in sdk\component\common\example\rtsp_client\example_rtsp_client.c
	static rtsp_client_params_t rtsp_client_params = {
		.type 		= AVMEDIA_TYPE_AUDIO,
		.codec_id 	= AV_CODEC_ID_PCMU,
		.channel 		= 1,
		.sample_rate 	= 24000,
		.cast_mode 	= UNICAST_UDP_MODE
	};

        4. When the ¡°rtp started (TCP)¡±or ¡°rtp started (UDP)¡± message shown on console, 
            it indicates that AmebaD is pushing the stream.