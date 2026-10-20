
TARGET = RANDOM
OBJS = main.o

INCDIR =
CFLAGS = -O2 -G0 -Wall
CXXFLAGS = $(CFLAGS) -fno-exceptions -fno-rtti
ASFLAGS = $(CFLAGS)

LIBS = -lpng -lz -lm

BUILD_PRX = 0
PSP_FW_VERSION = 500

EXTRA_TARGETS = EBOOT.PBP
PSP_EBOOT_TITLE = RANDOM

PSPSDK = $(shell psp-config --pspsdk-path)
include $(PSPSDK)/lib/build.mak
