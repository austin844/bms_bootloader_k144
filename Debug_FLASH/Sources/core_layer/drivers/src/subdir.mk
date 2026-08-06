################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../Sources/core_layer/drivers/src/rr_can.c \
../Sources/core_layer/drivers/src/rr_crc.c \
../Sources/core_layer/drivers/src/rr_gpio.c \
../Sources/core_layer/drivers/src/rr_iflash.c \
../Sources/core_layer/drivers/src/rr_memory.c \
../Sources/core_layer/drivers/src/rr_timer.c \
../Sources/core_layer/drivers/src/rr_watchdog.c 

OBJS += \
./Sources/core_layer/drivers/src/rr_can.o \
./Sources/core_layer/drivers/src/rr_crc.o \
./Sources/core_layer/drivers/src/rr_gpio.o \
./Sources/core_layer/drivers/src/rr_iflash.o \
./Sources/core_layer/drivers/src/rr_memory.o \
./Sources/core_layer/drivers/src/rr_timer.o \
./Sources/core_layer/drivers/src/rr_watchdog.o 

C_DEPS += \
./Sources/core_layer/drivers/src/rr_can.d \
./Sources/core_layer/drivers/src/rr_crc.d \
./Sources/core_layer/drivers/src/rr_gpio.d \
./Sources/core_layer/drivers/src/rr_iflash.d \
./Sources/core_layer/drivers/src/rr_memory.d \
./Sources/core_layer/drivers/src/rr_timer.d \
./Sources/core_layer/drivers/src/rr_watchdog.d 


# Each subdirectory must supply rules for building sources it contributes
Sources/core_layer/drivers/src/%.o: ../Sources/core_layer/drivers/src/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: Standard S32DS C Compiler'
	arm-none-eabi-gcc "@Sources/core_layer/drivers/src/rr_can.args" -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


