################################################################################
# Automatically-generated file. Do not edit!
################################################################################

-include ../makefile.init

RM := rm -rf

# All of the sources participating in the build are defined here
-include sources.mk
-include Sources/middle_layer/services/src/subdir.mk
-include Sources/middle_layer/communication_stack/src/subdir.mk
-include Sources/core_layer/drivers/src/subdir.mk
-include Sources/core_layer/bsp/nxp/src/subdir.mk
-include Sources/application_layer/app_uds/src/subdir.mk
-include Sources/application_layer/app_main/src/subdir.mk
-include Sources/subdir.mk
-include SDK/rtos/osif/subdir.mk
-include SDK/platform/pal/src/wdg/subdir.mk
-include SDK/platform/pal/src/timing/subdir.mk
-include SDK/platform/pal/src/can/subdir.mk
-include SDK/platform/drivers/src/power/S32K1xx/subdir.mk
-include SDK/platform/drivers/src/power/subdir.mk
-include SDK/platform/drivers/src/pins/subdir.mk
-include SDK/platform/drivers/src/interrupt/subdir.mk
-include SDK/platform/drivers/src/ftm/subdir.mk
-include SDK/platform/drivers/src/flexcan/subdir.mk
-include SDK/platform/drivers/src/flash/subdir.mk
-include SDK/platform/drivers/src/ewm/subdir.mk
-include SDK/platform/drivers/src/edma/subdir.mk
-include SDK/platform/drivers/src/crc/subdir.mk
-include SDK/platform/drivers/src/clock/S32K1xx/subdir.mk
-include SDK/platform/devices/S32K144/startup/subdir.mk
-include SDK/platform/devices/subdir.mk
-include Project_Settings/Startup_Code/subdir.mk
-include Generated_Code/subdir.mk
-include subdir.mk
-include objects.mk

ifneq ($(MAKECMDGOALS),clean)
ifneq ($(strip $(C_DEPS)),)
-include $(C_DEPS)
endif
endif

-include ../makefile.defs

# Add inputs and outputs from these tool invocations to the build variables 
SECONDARY_SIZE += \
bms_bootloader_k144.siz \


# All Target
all: bms_bootloader_k144.elf secondary-outputs

# Tool invocations
bms_bootloader_k144.elf: $(OBJS) C:/Users/austin_rideriver/bootloader_new_bms_k144/bms_bootloader_k144/Project_Settings/Linker_Files/S32K144_64_flash.ld $(USER_OBJS)
	@echo 'Building target: $@'
	@echo 'Invoking: Standard S32DS C Linker'
	arm-none-eabi-gcc -o "bms_bootloader_k144.elf" "@bms_bootloader_k144.args"  $(USER_OBJS) $(LIBS)
	@echo 'Finished building target: $@'
	@echo ' '

bms_bootloader_k144.siz: bms_bootloader_k144.elf
	@echo 'Invoking: Standard S32DS Print Size'
	arm-none-eabi-size --format=berkeley bms_bootloader_k144.elf
	@echo 'Finished building: $@'
	@echo ' '

# Other Targets
clean:
	-$(RM) ./*
	-@echo ' '

secondary-outputs: $(SECONDARY_SIZE)

.PHONY: all clean dependents

-include ../makefile.targets
