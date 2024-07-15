include $(MAKE_INCLUDE_GEN)

.PHONY: all clean

#*****************************************************************************#
#                                  VARIABLES                                  #
#*****************************************************************************#
DIR = $(BASEDIR)/component/common/bluetooth/realtek
MODULE_IFLAGS += \
	-I$(DIR)/sdk \
	-I$(DIR)/sdk/inc \
	-I$(DIR)/sdk/inc/app \
	-I$(DIR)/sdk/inc/bluetooth/gap \
	-I$(DIR)/sdk/inc/bluetooth/gap/gap_lib \
	-I$(DIR)/sdk/inc/os \
	-I$(DIR)/sdk/inc/platform \
	-I$(DIR)/sdk/inc/bluetooth/profile \
	-I$(DIR)/sdk/inc/bluetooth/profile/client \
	-I$(DIR)/sdk/inc/bluetooth/profile/server \
	-I$(DIR)/sdk/inc/stack \
	-I$(DIR)/sdk/board/amebad/src \
	-I$(DIR)/sdk/board/amebad/src/hci \
	-I$(DIR)/sdk/board/amebad/src/vendor_cmd \
	-I$(DIR)/sdk/board/amebad/lib \
	-I$(DIR)/sdk/board/common/inc

CSRC += \
	$(DIR)/sdk/board/common/os/freertos/osif_freertos.c \
	$(DIR)/sdk/board/amebad/src/platform_utils.c \
	$(DIR)/sdk/board/common/src/cycle_queue.c \
	$(DIR)/sdk/board/common/src/trace_task.c \
	$(DIR)/sdk/board/common/src/hci_process.c \
	$(DIR)/sdk/board/common/src/hci_adapter.c \
	$(DIR)/sdk/board/common/src/bt_uart_bridge.c \
	$(DIR)/sdk/board/amebad/src/trace_uart.c \
	$(DIR)/sdk/board/amebad/src/rtk_coex.c \
	$(DIR)/sdk/board/amebad/src/vendor_cmd/vendor_cmd.c \
	$(DIR)/sdk/board/amebad/src/hci/hci_uart.c \
	$(DIR)/sdk/board/amebad/src/hci/hci_board.c \
	$(DIR)/sdk/board/amebad/src/hci/bt_fwconfig.c \
	$(DIR)/sdk/board/amebad/src/hci/bt_normal_patch.c \
	$(DIR)/sdk/board/amebad/src/hci/bt_mp_patch.c \
	$(DIR)/sdk/bt_example_entry.c

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
