################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../Sources/application_layer/app_main/src/app_main.c 

OBJS += \
./Sources/application_layer/app_main/src/app_main.o 

C_DEPS += \
./Sources/application_layer/app_main/src/app_main.d 


# Each subdirectory must supply rules for building sources it contributes
Sources/application_layer/app_main/src/%.o: ../Sources/application_layer/app_main/src/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: Standard S32DS C Compiler'
	arm-none-eabi-gcc "@Sources/application_layer/app_main/src/app_main.args" -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


