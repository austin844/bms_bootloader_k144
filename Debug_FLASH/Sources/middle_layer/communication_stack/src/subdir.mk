################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../Sources/middle_layer/communication_stack/src/can_dbc.c \
../Sources/middle_layer/communication_stack/src/can_rx_comm.c \
../Sources/middle_layer/communication_stack/src/uds_can_transport.c \
../Sources/middle_layer/communication_stack/src/uds_timer_lib.c 

OBJS += \
./Sources/middle_layer/communication_stack/src/can_dbc.o \
./Sources/middle_layer/communication_stack/src/can_rx_comm.o \
./Sources/middle_layer/communication_stack/src/uds_can_transport.o \
./Sources/middle_layer/communication_stack/src/uds_timer_lib.o 

C_DEPS += \
./Sources/middle_layer/communication_stack/src/can_dbc.d \
./Sources/middle_layer/communication_stack/src/can_rx_comm.d \
./Sources/middle_layer/communication_stack/src/uds_can_transport.d \
./Sources/middle_layer/communication_stack/src/uds_timer_lib.d 


# Each subdirectory must supply rules for building sources it contributes
Sources/middle_layer/communication_stack/src/%.o: ../Sources/middle_layer/communication_stack/src/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: Standard S32DS C Compiler'
	arm-none-eabi-gcc "@Sources/middle_layer/communication_stack/src/can_dbc.args" -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


