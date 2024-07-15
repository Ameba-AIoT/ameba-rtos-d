
include $(MAKE_INCLUDE_GEN)

.PHONY: all test lib clean


#*****************************************************************************#
#                               Object FILE LIST                              #
#*****************************************************************************#
OBJS =

#*****************************************************************************#
#                        RULES TO GENERATE TARGETS                            #
#*****************************************************************************#

# Define the Rules to build the core targets
test: CORE_TARGETS
ifeq ($(CONFIG_AUDIO_MIXER),y)
	make -C lite/audio_stream/tests all
else ifeq ($(CONFIG_AUDIO_PASSTHROUGH),y)
	make -C passthrough/tests all
endif

lib: CORE_TARGETS
	make -C algorithms lib
	make -C lite lib
	make -C passthrough lib
	make -C xip lib
#*****************************************************************************#
#              GENERATE OBJECT FILE                                           #
#*****************************************************************************#
CORE_TARGETS: $(OBJS)




#*****************************************************************************#
#                        RULES TO CLEAN TARGETS                               #
#*****************************************************************************#
clean:
	make -C algorithms clean
	make -C lite clean
	make -C passthrough clean
	make -C lite/audio_stream/tests clean
	make -C passthrough/tests clean
	make -C xip clean
-include $(DEPS)
