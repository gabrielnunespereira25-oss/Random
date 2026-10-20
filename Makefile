
TARGET = RANDOM
OBJS = main.o

PSPSDK := $(shell psp-config --pspsdk-path)

CFLAGS = -O2 -G0 -Wall
ASFLAGS = $(CFLAGS)
LIBS = -lpng -lz -lm

BUILD_PRX = 0
EXTRA_TARGETS = EBOOT.PBP
PSP_EBOOT_TITLE = RANDOM

.PHONY: all clean

all: EBOOT.PBP

main.o: main.c
	$(CC) $(CFLAGS) -c main.c -o main.o

include $(PSPSDK)/lib/build.mak

clean:
	rm -f $(OBJS) $(TARGET).elf $(TARGET).prx PARAM.SFO EBOOT.PBP
