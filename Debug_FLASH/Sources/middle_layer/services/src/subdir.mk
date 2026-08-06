################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../Sources/middle_layer/services/src/service_can.c \
../Sources/middle_layer/services/src/service_iflash.c \
../Sources/middle_layer/services/src/service_timer.c \
../Sources/middle_layer/services/src/service_wdog.c 

OBJS += \
./Sources/middle_layer/services/src/service_can.o \
./Sources/middle_layer/services/src/service_iflash.o \
./Sources/middle_layer/services/src/service_timer.o \
./Sources/middle_layer/services/src/service_wdog.o 

C_DEPS += \
./Sources/middle_layer/services/src/service_can.d \
./Sources/middle_layer/services/src/service_iflash.d \
./Sources/middle_layer/services/src/service_timer.d \
./Sources/middle_layer/services/src/service_wdog.d 


# Each subdirectory must supply rules for building sources it contributes
Sources/middle_layer/services/src/%.o: ../Sources/middle_layer/services/src/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: Standard S32DS C Compiler'
	arm-none-eabi-gcc "@Sources/middle_layer/services/src/service_can.args" -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


