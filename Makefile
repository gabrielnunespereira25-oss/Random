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

SPRITE_FILES = \
    assets/sprites/idle_1.png \
    assets/sprites/idle_2.png \
    assets/sprites/walk_1.png \
    assets/sprites/walk_2.png \
    assets/sprites/run_1.png \
    assets/sprites/run_2.png \
    assets/sprites/run_3.png \
    assets/sprites/run_4.png \
    assets/sprites/jump_1.png \
    assets/sprites/jump_2.png \
    assets/sprites/fall.png \
    assets/sprites/attack_1.png \
    assets/sprites/attack_2.png \
    assets/sprites/magic.png

.PHONY: all clean check-sprites

ifeq ($(strip $(PSPSDK)),)
$(error PSPSDK nao encontrado)
endif

include $(PSPSDK)/lib/build.mak

check-sprites:
	@for f in $(SPRITE_FILES); do \
		if [ ! -f "$$f" ]; then \
			echo "FALTA PNG: $$f"; \
			exit 1; \
		fi; \
	done
	@echo "Todos os PNGs foram encontrados."

package-assets: check-sprites
	mkdir -p package/assets/sprites
	cp $(SPRITE_FILES) package/assets/sprites/
	cp EBOOT.PBP package/EBOOT.PBP
