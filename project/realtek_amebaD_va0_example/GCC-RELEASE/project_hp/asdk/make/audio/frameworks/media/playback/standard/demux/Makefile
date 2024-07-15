
include $(MAKE_INCLUDE_GEN)

.PHONY: all clean

#*****************************************************************************#
#                               VARIABLES	                              #
#*****************************************************************************#
DIR = $(BASEDIR)/component/audio/frameworks/media/playback/stardard
MODULE_IFLAGS += -I$(DIR)/demux/include
MODULE_IFLAGS += -I$(DIR)/utils/include
MODULE_IFLAGS += -I$(DIR)/common/include
MODULE_IFLAGS += -I$(DIR)/source/include
MODULE_IFLAGS += -I$(BASEDIR)/component/audio/base/libs/src/include
MODULE_IFLAGS += -I$(BASEDIR)/component/audio/base/log/interfaces
MODULE_IFLAGS += -I$(BASEDIR)/component/audio/base/osal/osal_c/interfaces
MODULE_IFLAGS += -I$(BASEDIR)/component/audio/base/osal/osal_cxx/interfaces
MODULE_IFLAGS += -I$(BASEDIR)/component/audio/base/cutils/interfaces
MODULE_IFLAGS += -I$(BASEDIR)/component/audio/base/utils/interfaces
MODULE_IFLAGS += -I$(BASEDIR)/component/audio/base/media/audio/audio_utils/include
MODULE_IFLAGS += -I$(BASEDIR)/component/audio/frameworks/media/playback/stardard/config/include

vpath %.cpp $(shell find $(DIR)/demux -type d)
#*****************************************************************************#
#                               CFLAGS                                        #
#*****************************************************************************#
GLOBAL_CFLAGS += -D__RTOS__ -Wno-multichar -Wno-unused-parameter -Wno-unused-value -Wno-unused-variable -Wno-unused-but-set-variable

#*****************************************************************************#
#                               Source FILE LIST                              #
#*****************************************************************************#
CSRC  = $(DIR)/demux/media_extractor.cpp
CSRC += $(DIR)/demux/media_extractor_factory.cpp

#CONFIG_MEDIA_DEMUX_AAC:=y
#CONFIG_MEDIA_DEMUX_WAV:=y
#CONFIG_MEDIA_DEMUX_MP3:=y
#CONFIG_MEDIA_DEMUX_MP4:=y

#HELIX AAC
ifeq ($(CONFIG_MEDIA_CODEC_HAAC),y)
GLOBAL_CFLAGS += -DMEDIA_DEMUX_AAC_DELIVER_ADTS_HEADER
endif

# AAC demux
ifeq ($(CONFIG_MEDIA_DEMUX_AAC),y)
LIBAAC_SRC_FILES := \
    $(DIR)/demux/extractors/aac/aac_extractor.cpp

CSRC += $(LIBAAC_SRC_FILES)
GLOBAL_CFLAGS += -DMEDIA_DEMUX_AAC
endif

# WAV demux
ifeq ($(CONFIG_MEDIA_DEMUX_WAV),y)
LIBWAV_SRC_FILES := \
    $(DIR)/demux/extractors/wav/wav_extractor.cpp

CSRC += $(LIBWAV_SRC_FILES)
GLOBAL_CFLAGS += -DMEDIA_DEMUX_WAV
endif

# MP3 demux
ifeq ($(CONFIG_MEDIA_DEMUX_MP3),y)
LIBMP3_SRC_FILES := \
    $(DIR)/demux/extractors/mp3/mp3_extractor.cpp \
    $(DIR)/demux/extractors/mp3/mp3_vbri_seeker.cpp \
    $(DIR)/demux/extractors/mp3/mpr_xing_seeker.cpp \
    $(DIR)/demux/extractors/mp3/mp3_id3.cpp

CSRC += $(LIBMP3_SRC_FILES)
GLOBAL_CFLAGS += -DMEDIA_DEMUX_MP3
endif

# AMR demux
ifeq ($(CONFIG_MEDIA_DEMUX_AMR),y)
LIBAMR_SRC_FILES := \
    $(DIR)/demux/extractors/amr/amr_extractor.cpp

CSRC += $(LIBAMR_SRC_FILES)
GLOBAL_CFLAGS += -DMEDIA_DEMUX_AMR
endif

# OGG demux
ifeq ($(CONFIG_MEDIA_DEMUX_OGG),y)
CSRC += $(DIR)/demux/extractors/ogg/ogg_extractor.cpp
MODULE_IFLAGS += -I$(BASEDIR)/component/audio/third_party/tremolo
GLOBAL_CFLAGS += -DMEDIA_DEMUX_OGG
endif

# FLAC demux
ifeq ($(CONFIG_MEDIA_DEMUX_FLAC),y)
CSRC += $(DIR)/demux/extractors/flac/flac_extractor.cpp
MODULE_IFLAGS += -I$(BASEDIR)/component/audio/third_party/flac/include
GLOBAL_CFLAGS += -DMEDIA_DEMUX_FLAC
endif

# MP4 demux
ifeq ($(CONFIG_MEDIA_DEMUX_MP4),y)
LIBMP4_SRC_FILES := \
    $(DIR)/demux/extractors/mp4/mpeg4_extractor.cpp \
    $(DIR)/demux/extractors/mp4/sample_table.cpp \
    $(DIR)/demux/extractors/mp4/sample_iterator.cpp

CSRC += $(LIBMP4_SRC_FILES)
GLOBAL_CFLAGS += -DMEDIA_DEMUX_MP4
endif

#*****************************************************************************#
#                               Object FILE LIST                              #
#*****************************************************************************#
OBJS = $(notdir $(CSRC:.cpp=.o))

#*****************************************************************************#
#                        RULES TO GENERATE TARGETS                            #
#*****************************************************************************#

# Define the Rules to build the core targets
COPY_RAM_OBJS: CORE_TARGETS
all: COPY_RAM_OBJS
	cp *.o  ../../xip

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
