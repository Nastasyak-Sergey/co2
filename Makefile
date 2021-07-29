# The main (project top) file without .c
TARGET = tester
# All source files go here:
SRCS = $(TARGET).c
# other sources added like that
SRCS += irq.c sched.c swtimer.c  debug.c common.c  board.c backup.c
SRCS +=  systick.c serial.c fifo.c i2c.c oled_ssd1306.c ssd1306_fonts.c #usb_conf.c msc.c cdc.c


# User defines
DEFINES =
# The libs which are linked to the resulting target
LIBS = -Wl,--start-group -lc -lgcc -Wl,--end-group
LIBS += -lopencm3
LIBS += -lprintf
#LIBS += -lm

# Possible values: debug, release
PROFILE = debug
# Use semihosting or not. Possible values: 0, 1
# Semihosting allows to pass printf() output and whole files between MCU and PC
# but the built target will not work without debugger connected
SEMIHOSTING ?= 0

# Be silent per default, but 'make V=1' will show all compiler calls.
ifneq ($(V),1) # 1
  Q		:= @
endif

# Optimization flags for debug build:
#   -Og -- optimize for debugging
#   -g3 -- include the most verbose debugging information into elf
#   -ggdb3 -- the same as g3, but the information is included in gdb format
OPTFLAGS_debug = -Og -ggdb3
# Optimization flags for release build: -Os -- optimize for smaller size
OPTFLAGS_release = -Os
# Optimization flags. Here we choose depending on profile
OPTFLAGS ?= ${OPTFLAGS_${PROFILE}}
# User flags should be given here
EXTRAFLAGS ?= $(OPTFLAGS) -std=gnu17 \
			  -Wall -Wextra -Wpedantic \
			  -Wimplicit-function-declaration -Wredundant-decls \
              -Wstrict-prototypes -Wundef -Wshadow

# Required by LCD lib to map charsets (UTF8 to CP1251)
EXTRAFLAGS += -finput-charset=UTF-8 -fexec-charset=cp1251
# Device is required for libopencm3
DEVICE ?= stm32f103c8t6
# Possible values: soft, hard
FPU ?= soft
FPU_FLAGS := -mfloat-abi=$(FPU) -msoft-float
# We want it built only for one MCU family to reduce build time
# See libopencm3 Makefile for details
LIBOPENCM3_TARGET ?= stm32/f1
# Directory with project sources
SRC_DIR ?= src
# Project include directories where project headers are placed
INC_DIRS ?= inc lib
# Directory where everything is built
BUILD_DIR ?= build
# Libraries should reside in one dir
LIB_DIR ?= lib
# This definition is used by Makefile includes for libopencm3
OPENCM3_DIR = $(LIB_DIR)/libopencm3
# Definitions required to generate linker script
include $(OPENCM3_DIR)/mk/genlink-config.mk

ARCHFLAGS := -mcpu=cortex-m3 -mthumb $(FPU_FLAGS) -mfix-cortex-m3-ldrd
CFLAGS := $(ARCHFLAGS)
CFLAGS += -fdata-sections -ffunction-sections
CFLAGS += -DUSE_SEMIHOSTING=$(SEMIHOSTING)
CFLAGS += $(addprefix -D,$(DEFINES)) $(genlink_cppflags) $(EXTRAFLAGS)

LDFLAGS := $(ARCHFLAGS) --static -nostartfiles
LDFLAGS += -Wl,-Map=$(BUILD_DIR)/$(PROFILE)/$(TARGET).map
#LDFLAGS += -nostdlib

ifeq ("$(SEMIHOSTING)","1")
LDFLAGS += --specs=rdimon.specs -lrdimon
else
LDFLAGS += -lnosys
endif

LDFLAGS += -L$(BUILD_DIR)/$(PROFILE) $(LIBS)
# Remove unused sections
ifneq ($(PROFILE),debug)
LDFLAGS += -Wl,--gc-sections -Wl,--print-gc-sections
endif

# Change this if using other toolchain
# Toolchain path could also be given here, i.e. /usr/bin/arm-none-eabi-
TOOLCHAIN_PREFIX ?= arm-none-eabi-

CC = $(TOOLCHAIN_PREFIX)gcc
CPP = $(TOOLCHAIN_PREFIX)g++
# Change to assembler-with-cpp if also using C++
AS = $(TOOLCHAIN_PREFIX)gcc -x assembler
CP = $(TOOLCHAIN_PREFIX)objcopy
SZ = $(TOOLCHAIN_PREFIX)size -d
GDB = $(TOOLCHAIN_PREFIX)gdb
OOCD ?= openocd -f interface/stlink-v2.cfg -f target/stm32f1x.cfg

HEX = $(CP) -O ihex -S
BIN = $(CP) -O binary -S

# Do not print "Entering directory ..." on recursive calls
MAKEFLAGS += --no-print-directory
# Automatically set flags for parallel build
# MAKEFLAGS += -j$(shell echo $$(($$(nproc)+1))) --load-average=$(shell nproc)

# Path to the the linker script
# This is used by libopencm3 makefile include
LDSCRIPT = $(BUILD_DIR)/$(DEVICE).ld

# All includes semi-automatically collected here
INCS = -I$(OPENCM3_DIR)/include $(addprefix -I,$(INC_DIRS))
OBJECTS = $(SRCS:.c=.o)

# where to place built object files
OBJDIR = $(BUILD_DIR)/$(PROFILE)/obj

# Default recipe. This one is executed when make is called without arguments
__DEFAULT: all

## Create build directories
$(BUILD_DIR):
	mkdir -p $@

$(BUILD_DIR)/$(PROFILE):
	mkdir -p $@

$(OBJDIR):
	mkdir -p $@

libprintf: $(BUILD_DIR)/$(PROFILE)/libprintf.a
$(BUILD_DIR)/$(PROFILE)/libprintf.a: $(OBJDIR) | $(OBJDIR)/libprintf.o
	$(AR) rcsv $@ $|

$(BUILD_DIR)/$(PROFILE)/obj/libprintf.o: $(LIB_DIR)/libprintf/printf.c
	@echo Building libprintf...
	$(CC) $(CFLAGS) $(INCS) -c $< -o $@


# Means that this target will trigger PROFILE change to release
$(BUILD_DIR)/libopencm3-docs: PROFILE=release
## Build libopencm3 documentation. Will be available in lib/libopencm3/doc
$(BUILD_DIR)/libopencm3-docs: $(OPENCM3_DIR)/Makefile | $(BUILD_DIR)/$(PROFILE)/libopencm3.a
	@echo Building libopencm3 documentation...
	cd $(OPENCM3_DIR) && $(MAKE) $(MAKEFLAGS) TARGETS="$(LIBOPENCM3_TARGET)" FP_FLAGS="$(FPU_FLAGS)" doc
	ln -sf $(OPENCM3_DIR)/doc $@

# alias
libopencm3-docs: $(BUILD_DIR)/libopencm3-docs

$(BUILD_DIR)/$(PROFILE)/libopencm3.a: | $(BUILD_DIR)/$(PROFILE) $(OPENCM3_DIR)/Makefile
	@echo Building libopencm3 for $(PROFILE) profile...
	cd $(OPENCM3_DIR) && $(MAKE) $(MAKEFLAGS) TARGETS="$(LIBOPENCM3_TARGET)" FP_FLAGS="$(FPU_FLAGS)" CFLAGS="$(CFLAGS)" V=1 clean lib
	cp $(OPENCM3_DIR)/lib/libopencm3_$(subst /,,$(LIBOPENCM3_TARGET)).a $@
	@echo libopencm3 for $(PROFILE) profile is built

# Include rules to generate linker script
include $(OPENCM3_DIR)/mk/genlink-rules.mk


## Recipe for building project object files, placed in separate directory
$(OBJDIR)/%.o: $(SRC_DIR)/%.c | $(OBJDIR) $(BUILD_DIR)/$(PROFILE)/libopencm3.a
	$(Q)$(CC) $(CFLAGS) $(INCS) -c $< -o $@

## Recipe for elf file, that is used for flashing and debugging, can be converted to bin/hex form
$(BUILD_DIR)/$(PROFILE)/$(TARGET).elf: $(addprefix $(OBJDIR)/,$(OBJECTS)) | \
$(BUILD_DIR)/$(PROFILE)/libopencm3.a \
$(LDSCRIPT)
	$(Q)$(CC) -T$(LDSCRIPT) $^ $(LDFLAGS) -o $@
	@echo
	$(SZ) $@
	@echo

## Generate .bin firmware from .elf
$(BUILD_DIR)/$(PROFILE)/$(TARGET).bin: $(BUILD_DIR)/$(PROFILE)/$(TARGET).elf
	$(Q)$(BIN) $< $@

## Generate .hex firmware from .elf
$(BUILD_DIR)/$(PROFILE)/$(TARGET).hex: $(BUILD_DIR)/$(PROFILE)/$(TARGET).elf
	$(Q)$(HEX) $< $@

## Flash MCU with currently built firmware
flash: $(BUILD_DIR)/$(PROFILE)/$(TARGET).elf
	$(OOCD) -c "program $< verify reset exit"

# Connect to the target in Debug mode
connect:
	$(Q)$(OOCD) -f scripts/connect.ocd

## Start GDB debug session
# Here we connect gdb and openocd via stdin-stdout pipe.
# This has an advantage of gdb starting and closing openocd without need for
# maintaining separate socket connection
# Alternative:
# - start openocd. It will listen for incoming connections on port 3333 by default
# - start gdb and connect as: 'target extended-remote localhost:3333' or simply 'tar ext :3333'
gdb: $(BUILD_DIR)/$(PROFILE)/$(TARGET).elf
ifeq ("$(SEMIHOSTING)","1")
	$(GDB) -ex 'target extended-remote | $(OOCD) -c "gdb_port pipe; init; arm semihosting enable; arm semihosting_fileio enable"' $<
else
	$(GDB) -ex 'target extended-remote | $(OOCD) -c "gdb_port pipe"' $<
endif


## Clean build directory for current profile and its build artefacts
clean:
	@echo Cleaning up...
#	$(Q)-rm -rf $(BUILD_DIR)/$(PROFILE)/$(TARGET).{elf,bin,hex,map} ## DOESNT work
	$(Q)-rm -rf $(OBJDIR)
	$(Q)find  $(BUILD_DIR)/$(PROFILE) \( -name '$(TARGET).elf' -o -name '$(TARGET).bin' -o -name '$(TARGET).hex' -o -name '$(TARGET).map' \) \
		-type f -exec rm {} +

## Remove everything created during builds
tidy: clean
	cd $(OPENCM3_DIR) && $(MAKE) TARGETS="$(LIBOPENCM3_TARGET)" V=1 clean
	-rm -rf $(BUILD_DIR)


## Build all
target $(TARGET): $(BUILD_DIR)/$(PROFILE)/$(TARGET).bin $(BUILD_DIR)/$(PROFILE)/$(TARGET).hex libprintf

## Alias for release build
## Catchall target. release-flash becomes make PROFILE=release flash
release-%: PROFILE=release
release-%:
	$(MAKE) PROFILE=$(PROFILE) $(patsubst release-%,%,$@)

## Alias for debug build
## Catchall target. release-gdb becomes make PROFILE=debug gdb
debug-%: PROFILE=debug
debug-%:
	$(MAKE) PROFILE=$(PROFILE) $(patsubst debug-%,%,$@)

all: | debug-$(TARGET) #release-$(TARGET) release-flash

.PHONY: __DEFAULT libopencm3-docs flash gdb clean tidy $(TARGET) target release-% debug-% all

