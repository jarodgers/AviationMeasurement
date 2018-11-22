################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../FatFS/src/diskio.c \
../FatFS/src/ff.c \
../FatFS/src/ffsystem.c \
../FatFS/src/ffunicode.c 

OBJS += \
./FatFS/src/diskio.o \
./FatFS/src/ff.o \
./FatFS/src/ffsystem.o \
./FatFS/src/ffunicode.o 

C_DEPS += \
./FatFS/src/diskio.d \
./FatFS/src/ff.d \
./FatFS/src/ffsystem.d \
./FatFS/src/ffunicode.d 


# Each subdirectory must supply rules for building sources it contributes
FatFS/src/%.o: ../FatFS/src/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: MCU GCC Compiler'
	@echo $(PWD)
	arm-none-eabi-gcc -mcpu=cortex-m4 -mthumb -mfloat-abi=hard -mfpu=fpv4-sp-d16 -DSTM32 -DSTM32F4 -DSTM32F446RETx -DNUCLEO_F446RE -DDEBUG -DSTM32F446xx -DUSE_STDPERIPH_DRIVER -I"C:/Users/jacob/Google Drive/Programming/eclipse-workspace/AviationMeasurement/StdPeriph_Driver/inc" -I"C:/Users/jacob/Google Drive/Programming/eclipse-workspace/AviationMeasurement/inc" -I"C:/Users/jacob/Google Drive/Programming/eclipse-workspace/AviationMeasurement/CMSIS/device" -I"C:/Users/jacob/Google Drive/Programming/eclipse-workspace/AviationMeasurement/CMSIS/core" -I"C:/Users/jacob/Google Drive/Programming/eclipse-workspace/AviationMeasurement/FreeRTOS/inc" -I"C:/Users/jacob/Google Drive/Programming/eclipse-workspace/AviationMeasurement/FatFS/inc" -O0 -g3 -Wall -fmessage-length=0 -ffunction-sections -c -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


