
#include <pspkernel.h>
#include <pspdisplay.h>
#include <pspctrl.h>
#include <pspdebug.h>
#include <pspgu.h>
#include <pspgum.h>
#include <string.h>

PSP_MODULE_INFO("RANDOM", 0, 1, 0);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER);

#define SCREEN_W 480
#define SCREEN_H 272
#define BUF_W 512

static unsigned int __attribute__((aligned(16))) list[262144];

static int running = 1;
static int selected = 0;

static const char *menu[] = {
    "JOGAR",
    "PERSONAGEM",
    "OPCOES",
    "SAIR"
};

static void setup_gu(void)
{
    sceGuInit();

    sceGuStart(GU_DIRECT, list);
    sceGuDrawBuffer(
        GU_PSM_8888, (void *)0, BUF_W
    );
    sceGuDispBuffer(
        SCREEN_W, SCREEN_H,
        (void *)0x88000, BUF_W
    );
    sceGuDepthBuffer(
        (void *)0x110000, BUF_W
    );

    sceGuOffset(2048 - SCREEN_W / 2,
                2048 - SCREEN_H / 2);
    sceGuViewport(2048, 2048,
                  SCREEN_W, SCREEN_H);
    sceGuDepthRange(65535, 0);

    sceGuScissor(0, 0, SCREEN_W, SCREEN_H);
    sceGuEnable(GU_SCISSOR_TEST);
    sceGuDisable(GU_DEPTH_TEST);
    sceGuDisable(GU_TEXTURE_2D);

    sceGuClearColor(0xFF000000);
    sceGuClear(GU_COLOR_BUFFER_BIT |
               GU_DEPTH_BUFFER_BIT);

    sceGuFinish();
    sceGuSync(0, 0);

    sceDisplayWaitVblankStart();
    sceGuDisplay(GU_TRUE);
}

static void draw_background(void)
{
    sceGuClearColor(0xFF101018);
    sceGuClear(GU_COLOR_BUFFER_BIT |
               GU_DEPTH_BUFFER_BIT);
}

int main(void)
{
    pspDebugScreenInit();
    sceCtrlSetSamplingCycle(0);
    sceCtrlSetSamplingMode(PSP_CTRL_MODE_ANALOG);

    setup_gu();

    while (running) {
        SceCtrlData pad;
        memset(&pad, 0, sizeof(pad));
        sceCtrlReadBufferPositive(&pad, 1);

        if (pad.Buttons & PSP_CTRL_UP) {
            selected--;
            if (selected < 0)
                selected = 3;
            sceKernelDelayThread(150000);
        }

        if (pad.Buttons & PSP_CTRL_DOWN) {
            selected++;
            if (selected > 3)
                selected = 0;
            sceKernelDelayThread(150000);
        }

        if (pad.Buttons & PSP_CTRL_CROSS) {
            if (selected == 3) {
                running = 0;
            } else {
                sceKernelDelayThread(150000);
            }
        }

        if (pad.Buttons & PSP_CTRL_CIRCLE) {
            running = 0;
        }

        draw_background();

        sceGuFinish();
        sceGuSync(0, 0);
        sceDisplayWaitVblankStart();
        sceGuSwapBuffers();

        pspDebugScreenSetXY(2, 2);
        pspDebugScreenSetTextColor(0xFFFFFFFF);
        pspDebugScreenPrintf("RANDOM PSP\n\n");

        for (int i = 0; i < 4; i++) {
            if (i == selected)
                pspDebugScreenSetTextColor(0xFF00FFFF);
            else
                pspDebugScreenSetTextColor(0xFFFFFFFF);

            pspDebugScreenPrintf(
                "%s %s\n",
                i == selected ? ">" : " ",
                menu[i]
            );
        }

        pspDebugScreenSetTextColor(0xFFFFFFFF);
        pspDebugScreenPrintf(
            "\nCROSS: selecionar\nCIRCLE: sair\n"
        );
    }

    sceGuTerm();
    sceKernelExitGame();
    return 0;
}
name: Build RANDOM PSP

on:
  workflow_dispatch:
  push:
    paths:
      - "src/**"
      - "assets/**"
      - "Makefile"
      - ".github/workflows/build.yml"

jobs:
  build:
    runs-on: ubuntu-latest
    container:
      image: pspdev/pspdev:latest

    steps:
      - name: Checkout
        uses: actions/checkout@v4

      - name: Check project files
        run: |
          test -f Makefile
          test -f src/main.c
          test -f src/character.c
          test -f src/character.h
          test -d assets/sprites

      - name: Build EBOOT
        run: |
          make clean || true
          make
          test -f EBOOT.PBP

      - name: Upload EBOOT
        uses: actions/upload-artifact@v4
        with:
          name: RANDOM-PSP-EBOOT
          path: EBOOT.PBP

      - name: Prepare package files
        run: |
          mkdir -p package/PSP/GAME/RANDOM/assets/sprites
          cp EBOOT.PBP package/PSP/GAME/RANDOM/
          cp assets/sprites/*.png package/PSP/GAME/RANDOM/assets/sprites/

      - name: Upload package files
        uses: actions/upload-artifact@v4
        with:
          name: RANDOM-PSP-ARQUIVOS
          path: package/PSP/
