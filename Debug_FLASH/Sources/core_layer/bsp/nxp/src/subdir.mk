################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../Sources/core_layer/bsp/nxp/src/rr_can_nxp.c \
../Sources/core_layer/bsp/nxp/src/rr_crc_nxp.c \
../Sources/core_layer/bsp/nxp/src/rr_gpio_nxp.c \
../Sources/core_layer/bsp/nxp/src/rr_iflash_nxp.c \
../Sources/core_layer/bsp/nxp/src/rr_timer_nxp.c \
../Sources/core_layer/bsp/nxp/src/rr_watchdog_nxp.c 

OBJS += \
./Sources/core_layer/bsp/nxp/src/rr_can_nxp.o \
./Sources/core_layer/bsp/nxp/src/rr_crc_nxp.o \
./Sources/core_layer/bsp/nxp/src/rr_gpio_nxp.o \
./Sources/core_layer/bsp/nxp/src/rr_iflash_nxp.o \
./Sources/core_layer/bsp/nxp/src/rr_timer_nxp.o \
./Sources/core_layer/bsp/nxp/src/rr_watchdog_nxp.o 

C_DEPS += \
./Sources/core_layer/bsp/nxp/src/rr_can_nxp.d \
./Sources/core_layer/bsp/nxp/src/rr_crc_nxp.d \
./Sources/core_layer/bsp/nxp/src/rr_gpio_nxp.d \
./Sources/core_layer/bsp/nxp/src/rr_iflash_nxp.d \
./Sources/core_layer/bsp/nxp/src/rr_timer_nxp.d \
./Sources/core_layer/bsp/nxp/src/rr_watchdog_nxp.d 


# Each subdirectory must supply rules for building sources it contributes
Sources/core_layer/bsp/nxp/src/%.o: ../Sources/core_layer/bsp/nxp/src/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: Standard S32DS C Compiler'
	arm-none-eabi-gcc "@Sources/core_layer/bsp/nxp/src/rr_can_nxp.args" -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


