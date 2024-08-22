include $(MAKE_INCLUDE_GEN)

.PHONY: all clean

#*****************************************************************************#
#                                  VARIABLES                                  #
#*****************************************************************************#
DIR = $(BASEDIR)/component/common/bluetooth/realtek
MODULE_IFLAGS += \
	-I$(DIR)/sdk/example/bt_ota/bt_ota_client \
	-I$(DIR)/sdk/example/bt_ota/platform/amebad

CSRC += \
	$(DIR)/sdk/example/bt_ota/bt_ota_client/bt_ota_client_app_main.c \
	$(DIR)/sdk/example/bt_ota/bt_ota_client/bt_ota_client_app_task.c \
	$(DIR)/sdk/example/bt_ota/bt_ota_client/bt_ota_client_app.c \
	$(DIR)/sdk/example/bt_ota/bt_ota_client/bt_ota_client_link_mgr.c \
	$(DIR)/sdk/example/bt_ota/bt_ota_client/bt_ota_gatt_client.c \
	$(DIR)/sdk/example/bt_ota/platform/amebad/bt_ota_client_platform.c

OBJS = $(CSRC:.c=.o)
TOBJS = $(notdir $(OBJS))
LOBJS = $(addprefix objs/,$(TOBJS))

#*****************************************************************************#
#                          RULES TO GENERATE TARGETS                          #
#*****************************************************************************#

# LIBBLUETOOTH_OBJS_PATH = $(shell pwd)/objs
# Define the Rules to build the core targets
all: CORE_TARGETS COPY_RAM_OBJS
	# mkdir -p $(LIBBLUETOOTH_OBJS_PATH)
	# for i in $(OBJS); do \
	# 	 $(COPY)  $$i $(LIBBLUETOOTH_OBJS_PATH) -f;\
	# 	done
	# $(REMOVE) *.i
	# $(REMOVE) *.s
	# $(REMOVE) *.d

#*****************************************************************************#
#                            GENERATE OBJECT FILE                             #
#*****************************************************************************#
CORE_TARGETS: $(OBJS)

#*****************************************************************************#
#                           RULES TO CLEAN TARGETS                            #
#*****************************************************************************#
clean: CLEAN_OBJS
	$(REMOVE) *.o
	$(REMOVE) *.i
	$(REMOVE) *.s
	$(REMOVE) *.d

-include $(DEPS)
