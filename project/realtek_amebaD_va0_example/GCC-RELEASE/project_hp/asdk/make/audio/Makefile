
include $(MAKE_INCLUDE_GEN)

.PHONY: all lib clean test


#*****************************************************************************#
#                               Object FILE LIST                              #
#*****************************************************************************#
OBJS = 

#*****************************************************************************#
#                        RULES TO GENERATE TARGETS                            #
#*****************************************************************************#

# Define the Rules to build the core targets
all: CORE_TARGETS
ifeq ($(CONFIG_BAIDU_DUER),y)
	make -C mp3 all
endif
ifeq ($(CONFIG_MEDIA_PLAYER),y)
	make -C frameworks/media/playback/standard/config
endif

ifeq ($(CONFIG_AUDIO_FWK),y)
	make -C hal/config all
endif

test:
ifeq ($(CONFIG_AUDIO_FWK),y)
	#make -C frameworks/media/audio test
	#make -C hal test
endif
ifeq ($(CONFIG_MEDIA_PLAYER),y)
	#make -C frameworks/media/playback tests
endif

lib:
ifeq ($(CONFIG_AUDIO_FWK),y)
	make -C hal lib
	make -C base lib
	make -C frameworks audiolib
endif
ifeq ($(CONFIG_MEDIA_PLAYER),y)
	make -C base lib
	make -C frameworks playerlib
	make -C 3rdparty lib
	make -C third_party lib
endif

#*****************************************************************************#
#              GENERATE OBJECT FILE                                           #
#*****************************************************************************#
CORE_TARGETS: $(OBJS)




#*****************************************************************************#
#                        RULES TO CLEAN TARGETS                               #
#*****************************************************************************#
clean: 
	make -C mp3 clean
	make -C hal clean
	make -C base clean
	make -C frameworks clean
	make -C 3rdparty clean
	make -C third_party clean
	make -C frameworks/media/playback/standard/config clean
	make -C hal/config clean

-include $(DEPS)
