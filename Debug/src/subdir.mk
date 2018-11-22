################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../src/amg.c \
../src/gps.c \
../src/i2c.c \
../src/lcd.c \
../src/main.c \
../src/sd.c \
../src/syscalls.c \
../src/system_stm32f4xx.c \
../src/tph.c 

OBJS += \
./src/amg.o \
./src/gps.o \
./src/i2c.o \
./src/lcd.o \
./src/main.o \
./src/sd.o \
./src/syscalls.o \
./src/system_stm32f4xx.o \
./src/tph.o 

C_DEPS += \
./src/amg.d \
./src/gps.d \
./src/i2c.d \
./src/lcd.d \
./src/main.d \
./src/sd.d \
./src/syscalls.d \
./src/system_stm32f4xx.d \
./src/tph.d 


# Each subdirectory must supply rules for building sources it contributes
src/%.o: ../src/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: MCU GCC Compiler'
	@echo $(PWD)
	arm-none-eabi-gcc -mcpu=cortex-m4 -mthumb -mfloat-abi=hard -mfpu=fpv4-sp-d16 -DSTM32 -DSTM32F4 -DSTM32F446RETx -DNUCLEO_F446RE -DDEBUG -DSTM32F446xx -DUSE_STDPERIPH_DRIVER -I"C:/Users/jacob/Google Drive/Programming/eclipse-workspace/AviationMeasurement/StdPeriph_Driver/inc" -I"C:/Users/jacob/Google Drive/Programming/eclipse-workspace/AviationMeasurement/inc" -I"C:/Users/jacob/Google Drive/Programming/eclipse-workspace/AviationMeasurement/CMSIS/device" -I"C:/Users/jacob/Google Drive/Programming/eclipse-workspace/AviationMeasurement/CMSIS/core" -I"C:/Users/jacob/Google Drive/Programming/eclipse-workspace/AviationMeasurement/FreeRTOS/inc" -I"C:/Users/jacob/Google Drive/Programming/eclipse-workspace/AviationMeasurement/FatFS/inc" -O0 -g3 -Wall -fmessage-length=0 -ffunction-sections -c -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


