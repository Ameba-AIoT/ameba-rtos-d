
include $(MAKE_INCLUDE_GEN)

.PHONY: all clean

#*****************************************************************************#
#                               VARIABLES	                              #
#*****************************************************************************#
DIR = $(BASEDIR)/component/audio/frameworks/media/playback/stardard

MODULE_IFLAGS += \
	-I$(BASEDIR)/component/audio/frameworks/media/playback/stardard/config/include

vpath %.cpp $(shell find $(DIR)/config -type d)
#*****************************************************************************#
#                               CFLAGS                                        #
#*****************************************************************************#
GLOBAL_CFLAGS += -D__RTOS__ -Wno-multichar -Wno-unused-parameter -std=gnu++17 -std=c++17

#*****************************************************************************#
#                               Source FILE LIST                              #
#*****************************************************************************#
CPPSRC = $(DIR)/config/media_config.cpp

#HELIC AAC
ifeq ($(CONFIG_MEDIA_CODEC_HAAC),y)
GLOBAL_CFLAGS += -DMEDIA_DEMUX_HELIC
endif

# AAC demux
ifeq ($(CONFIG_MEDIA_DEMUX_AAC),y)
GLOBAL_CFLAGS += -DMEDIA_DEMUX_AAC
endif

# WAV demux
ifeq ($(CONFIG_MEDIA_DEMUX_WAV),y)
GLOBAL_CFLAGS += -DMEDIA_DEMUX_WAV
endif

# MP3 demux
ifeq ($(CONFIG_MEDIA_DEMUX_MP3),y)
GLOBAL_CFLAGS += -DMEDIA_DEMUX_MP3
endif

# AMR demux
ifeq ($(CONFIG_MEDIA_DEMUX_AMR),y)
GLOBAL_CFLAGS += -DMEDIA_DEMUX_AMR
endif

# OGG demux
ifeq ($(CONFIG_MEDIA_DEMUX_OGG),y)
GLOBAL_CFLAGS += -DMEDIA_DEMUX_OGG
endif

# FLAC demux
ifeq ($(CONFIG_MEDIA_DEMUX_FLAC),y)
GLOBAL_CFLAGS += -DMEDIA_DEMUX_FLAC
endif

# MP4 demux
ifeq ($(CONFIG_MEDIA_DEMUX_MP4),y)
GLOBAL_CFLAGS += -DMEDIA_DEMUX_MP4
endif

# rawpcm codec
ifeq ($(CONFIG_MEDIA_CODEC_PCM),y)
GLOBAL_CFLAGS += -DMEDIA_CODEC_PCM
endif

# mp3 codec
ifeq ($(CONFIG_MEDIA_CODEC_MP3),y)
GLOBAL_CFLAGS += -DMEDIA_CODEC_MP3
endif

# aac codec
ifeq ($(CONFIG_MEDIA_CODEC_AAC),y)
GLOBAL_CFLAGS += -DMEDIA_CODEC_AAC
endif

# haac codec
ifeq ($(CONFIG_MEDIA_CODEC_HAAC),y)
GLOBAL_CFLAGS += -DMEDIA_CODEC_HAAC
endif

#*****************************************************************************#
#                               Object FILE LIST                              #
#*****************************************************************************#
#OBJS = $(notdir $(CSRC:.c=.o))
OBJS = $(notdir $(CPPSRC:.cpp=.o))

#*****************************************************************************#
#                        RULES TO GENERATE TARGETS                            #
#*****************************************************************************#

# Define the Rules to build the core targets
COPY_RAM_OBJS: CORE_TARGETS
all: COPY_RAM_OBJS

#*****************************************************************************#
#              GENERATE OBJECT FILE
#*****************************************************************************#
CORE_TARGETS: $(OBJS)


#*****************************************************************************#
#                        RULES TO CLEAN TARGETS                               #
#*****************************************************************************#
clean: CLEAN_OBJS
	$(REMOVE) *.o
	$(REMOVE) *.i
	$(REMOVE) *.ii
	$(REMOVE) *.s
	$(REMOVE) *.d
	$(REMOVE) *.su

-include $(DEPS)
