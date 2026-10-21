TARGET = RANDOM
TITLE = RANDOM
BUILD_PRX = 1

PSPSDK := $(shell psp-config --pspsdk-path)

INCDIR = src

CFLAGS = -O2 -G0 -Wall -std=c99
CXXFLAGS = $(CFLAGS)
ASFLAGS = $(CFLAGS)

OBJS = \
    src/main.o \
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

PSP_EBOOT_ICON = ICON0.PNG

ifeq ($(strip $(PSPSDK)),)
$(error PSPSDK nao encontrado)
endif

.PHONY: all clean package-assets

all: $(EXTRA_TARGETS)

package-assets:
	mkdir -p assets/sprites

clean:
	$(MAKE) -f $(PSPSDK)/lib/build.mak clean
	rm -f EBOOT.PBP

include $(PSPSDK)/lib/build.mak
