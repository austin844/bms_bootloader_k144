################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../Sources/application_layer/app_uds/src/boot_metadata.c \
../Sources/application_layer/app_uds/src/uds.c 

OBJS += \
./Sources/application_layer/app_uds/src/boot_metadata.o \
./Sources/application_layer/app_uds/src/uds.o 

C_DEPS += \
./Sources/application_layer/app_uds/src/boot_metadata.d \
./Sources/application_layer/app_uds/src/uds.d 


# Each subdirectory must supply rules for building sources it contributes
Sources/application_layer/app_uds/src/%.o: ../Sources/application_layer/app_uds/src/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: Standard S32DS C Compiler'
	arm-none-eabi-gcc "@Sources/application_layer/app_uds/src/boot_metadata.args" -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


