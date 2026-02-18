PROJECT_NAME     := RGBSA
TARGETS          := RGBSA_52832
OUTPUT_DIRECTORY := build
SDK_ROOT := sdk

# Apply the linker script to all defined targets
$(foreach target, $(TARGETS), $(eval $(target): LINKER_SCRIPT := loader.ld))

SRC_FILES += \
  $(SDK_ROOT)/modules/nrfx/mdk/gcc_startup_nrf52.S \
  $(SDK_ROOT)/modules/nrfx/mdk/system_nrf52.c \
  $(SDK_ROOT)/components/boards/boards.c \
  main.c \

INC_FOLDERS += \
  $(SDK_ROOT)/components/softdevice/s112/headers \
  $(SDK_ROOT)/components/libraries/util \
  $(SDK_ROOT)/modules/nrfx \
  $(SDK_ROOT)/integration/nrfx \
  $(SDK_ROOT)/components/libraries/delay \
  $(SDK_ROOT)/components/boards \
  . \
  $(SDK_ROOT)/modules/nrfx/hal \
  $(SDK_ROOT)/components/toolchain/cmsis/include \
  $(SDK_ROOT)/modules/nrfx/mdk \

OPT = -Os -g3

CFLAGS += $(OPT)
CFLAGS += -DCONFIG_GPIO_AS_PINRESET
CFLAGS += -DFLOAT_ABI_HARD
CFLAGS += -DNRF52
CFLAGS += -DNRF52832_XXAA
CFLAGS += -DNRF52_PAN_74
CFLAGS += -mcpu=cortex-m4
CFLAGS += -mthumb -mabi=aapcs
CFLAGS += -mfloat-abi=hard -mfpu=fpv4-sp-d16
CFLAGS += -ffunction-sections -fdata-sections -fno-strict-aliasing
CFLAGS += -fno-builtin -fshort-enums
CFLAGS += -DBOARD_CUSTOM

CXXFLAGS += $(OPT)
ASMFLAGS += -g3
ASMFLAGS += -mcpu=cortex-m4
ASMFLAGS += -mthumb -mabi=aapcs
ASMFLAGS += -mfloat-abi=hard -mfpu=fpv4-sp-d16
ASMFLAGS += -DCONFIG_GPIO_AS_PINRESET
ASMFLAGS += -DFLOAT_ABI_HARD
ASMFLAGS += -DNRF52
ASMFLAGS += -DNRF52832_XXAA
ASMFLAGS += -DNRF52_PAN_74
ASMFLAGS += -DBOARD_CUSTOM

LDFLAGS += $(OPT)
LDFLAGS += -mthumb -mabi=aapcs -L$(SDK_ROOT)/modules/nrfx/mdk -T$(LINKER_SCRIPT)
LDFLAGS += -mcpu=cortex-m4
LDFLAGS += -mfloat-abi=hard -mfpu=fpv4-sp-d16
LDFLAGS += -Wl,--gc-sections
LDFLAGS += --specs=nano.specs

# Target Specific Flags
# ---------------------
$(TARGETS): ASMFLAGS += -D__HEAP_SIZE=0

LIB_FILES += -lc -lnosys -lm

# -------------------------------------------------------------------------
# Default target
# 1. Builds all targets
# 2. Generates OTA .zip files for all targets
# -------------------------------------------------------------------------
default: $(TARGETS)
	@echo "------------------------------------------------"
	@echo "Generating OTA Packages (.zip) for all targets..."
	@echo "------------------------------------------------"
	$(foreach target, $(TARGETS), \
		echo "Creating $(target).zip ..."; \
		nrfutil pkg generate --application $(OUTPUT_DIRECTORY)/$(target).hex --application-version 99 --hw-version 52 --sd-req 0x0103 --key-file priv.pem $(target).zip; \
	)
	@echo "Done."

TEMPLATE_PATH := $(SDK_ROOT)/components/toolchain/gcc
include $(TEMPLATE_PATH)/Makefile.common

$(foreach target, $(TARGETS), $(call define_target, $(target)))

.PHONY: flash

flash: default
	@echo "------------------------------------------------"
	@echo "Flashing Target: RGBSA using OpenOCD"
	@echo "------------------------------------------------"
	openocd -f interface/ultralink-NRF52.cfg -c "init; halt; nrf52_flash_bank_erase 0; flash write_image $(OUTPUT_DIRECTORY)/RGBSA_52832.hex; reset; exit"
