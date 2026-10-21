
TARGET = RANDOM
TITLE = RANDOM
BUILD_PRX = 1

PSPSDK := $(shell psp-config --pspsdk-path)

INCDIR = src
CFLAGS = -O2 -G0 -Wall -std=c99 -D_PSP_FW_VERSION=600
CXXFLAGS = $(CFLAGS)
ASFLAGS = $(CFLAGS)

OBJS = \
    src/main.o \
    src/menu.o \
    src/game.o \
    src/graphics.o \
    src/sprite.o \
    src/character.o

LIBS = \
    -lpng \
    -lz \
    -lm \
    -lpspgum \
    -lpspgu \
    -lpspdebug \
    -lpspdisplay \
    -lpspge \
    -lpspctrl \
    -lpspkernel

EXTRA_TARGETS = EBOOT.PBP
PSP_EBOOT_TITLE = RANDOM

ifeq ($(strip $(PSPSDK)),)
$(error PSPSDK nao encontrado)
endif

include $(PSPSDK)/lib/build.mak
