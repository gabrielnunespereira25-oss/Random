TARGET = RANDOM
OBJS = src/main.o

BUILD_PRX = 1
PSP_FW_VERSION = 660
EXTRA_TARGETS = EBOOT.PBP
PSP_EBOOT_TITLE = RANDOM Beta 0.1

.PHONY: all clean

all: EBOOT.PBP

clean:
	$(RM) $(OBJS) $(TARGET).elf $(TARGET).prx PARAM.SFO EBOOT.PBP

PSPSDK := $(shell psp-config --pspsdk-path)
include $(PSPSDK)/lib/build.mak
