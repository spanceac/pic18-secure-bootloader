CC=/home/spanceac/sebu/MPLAB-install/XC8-install/v2.41/bin/xc8-cc

OFFSET=0x1000

all: bootloader

bootloader: main.c sha256/sha256.c uECC/uECC.c
	$(CC) -mcodeoffset=$(OFFSET) -ginhx32 -mcpu=18F25K22 main.c sha256/sha256.c uECC/uECC.c -O0 -o bootloader
	python ../tools/btld-patch.py bootloader.hex $(OFFSET)
