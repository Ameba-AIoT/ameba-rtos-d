
include $(MAKE_INCLUDE_GEN)

.PHONY: all lib lib_mp lib_lp lib_lt clean

TARGET_LIB = lib_wlan
#*****************************************************************************#
#                               Object FILE LIST                              #
#*****************************************************************************#
OBJS = 

#*****************************************************************************#
#                        RULES TO GENERATE TARGETS                            #
#*****************************************************************************#

# Define the Rules to build the core targets
lib: GEN_GIT_VERSION_FILE CORE_TARGETS
	make -C sram lib
	make -C xip lib
	$(REMOVE) $(TARGET_LIB)_git_version.*
	
lib_mp: GEN_GIT_VERSION_FILE CORE_TARGETS
	make -C sram lib_mp
	make -C xip lib_mp
	$(REMOVE) $(TARGET_LIB)_git_version.*
	
lib_lp: GEN_GIT_VERSION_FILE CORE_TARGETS
	make -C sram lib_lp
	make -C xip lib_lp
	$(REMOVE) $(TARGET_LIB)_git_version.*

lib_lt: GEN_GIT_VERSION_FILE CORE_TARGETS
	make -C sram lib_lt
	make -C xip lib_lt
	$(REMOVE) $(TARGET_LIB)_git_version.*
	
all:
	make -C option all

#*****************************************************************************#
#              GENERATE OBJECT FILE 
#*****************************************************************************#
CORE_TARGETS: $(OBJS)

#*****************************************************************************#
#              GENERATE GIT VERSION FILE                                      #
#*****************************************************************************#
GEN_GIT_VERSION_FILE:
	$(call GEN_GIT_VER, $(TARGET_LIB))

#*****************************************************************************#
#                        RULES TO CLEAN TARGETS                               #
#*****************************************************************************#
clean: 
	make -C sram clean
	make -C xip clean
	make -C option clean
	



