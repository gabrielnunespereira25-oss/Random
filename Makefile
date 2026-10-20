
TARGET = RANDOM
OBJS = src/main.o

CFLAGS = -O2 -G0 -Wall
ASFLAGS = $(CFLAGS)
LIBS = -lpng -lz -lm

BUILD_PRX = 0
EXTRA_TARGETS = EBOOT.PBP
PSP_EBOOT_TITLE = RANDOM

PSPSDK = $(shell psp-config --pspsdk-path)
include $(PSPSDK)/lib/build.mak
