
TARGET = RANDOM
OBJS = src/main.o

INCDIR =
CFLAGS = -O2 -G0 -Wall
CXXFLAGS = $(CFLAGS) -fno-exceptions -fno-rtti
ASFLAGS = $(CFLAGS)

BUILD_PRX = 1
PSP_FW_VERSION = 660
EXTRA_TARGETS = EBOOT.PBP
PSP_EBOOT_TITLE = RANDOM Beta 0.1

LIBS = -lpspdebug -lpspdisplay -lpspctrl -lpspkernel

PSPSDK := $(shell psp-config --pspsdk-path)
include $(PSPSDK)/lib/build.mak

.PHONY: check-source

check-source:
	@echo "Arquivos na raiz:"
	@ls -la
	@echo "Arquivos em src:"
	@ls -la src
