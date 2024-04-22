ARM_CC=arm-linux-gnueabihf-gcc 
CFLAGS=-Wall -fno-stack-protector -pie

all: processor_arm sensor_arm

processor_arm: processor.c
	$(ARM_CC) $(CFLAGS) processor.c -o processor_arm

sensor_arm: sensor.c
	$(ARM_CC) $(CFLAGS) sensor.c -o sensor_arm


clean:
	rm -f processor_arm sensor_arm