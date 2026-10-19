/*
 * RANDOM ALPHA
 * Prototipo PSP: menu ate o primeiro chefe
 * Requer PSPSDK
 */

#include <pspkernel.h>
#include <pspdisplay.h>
#include <pspctrl.h>
#include <pspgu.h>
#include <math.h>

PSP_MODULE_INFO("RANDOM", 0, 1, 0);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER);

#define SW 480
#define SH 272
#define WW 1600
#define GROUND 250
#define PW 18
#define PH 30

typedef unsigned int Color;
typedef struct { Color c; float x,y,z; } Vtx;
typedef struct { float x,y,w,h; } Box;

static unsigned int __attribute__((aligned(16))) list[262144];

/* Paleta */
#define BLACK  0xFF101018
#define DARK   0xFF252535
#define STONE  0xFF555568
#define GRAY   0xFF858596
#define LIGHT  0xFFBFC4D0
#define WHITE  0xFFFFFFFF
#define PURPLE 0xFFB050E8
#define RED    0xFF3030E8
#define YELLOW 0xFF30E8F8
#define GREEN  0xFF35B85A
#define BLUE   0xFFFFA040

static float px=60, py=220, vy=0, cam=0;
static int face=1, jumps=2, hp=5, maxHP=5;
static int magic=0, magicCD=0, swordCD=0;
static int xp=0, level=1, nextXP=20;
static int bossHP=20, bossActive=1, bossStarted=0;
static int gameMode=0, menuChoice=0, dialogStep=0;
static int paused=0;

enum { MENU, GAME, DIALOG, PAUSE, WIN };

static void rect(float x,float y,float w,float h,Color c);
static void text5(int x,int y,const char *s,Color c,int scale);

static int pressed(unsigned int n,unsigned int o,unsigned int b) {
    return (n&b)&&!(o&b);
}

/* Fonte bitmap 5x7: A-Z, 0-9 e espaco */
static const unsigned char font5x7[36][7]={
{14,17,17,31,17,17,17},{30,17,17,30,17,17,30},
{14,17,16,16,16,17,14},{30,17,17,17,17,17,30},
{31,16,16,30,16,16,31},{31,16,16,30,16,16,16},
{14,17,16,23,17,17,15},{17,17,17,31,17,17,17},
{14,4,4,4,4,4,14},{7,2,2,2,18,18,12},
{17,18,20,24,20,18,17},{16,16,16,16,16,16,31},
{17,27,21,21,17,17,17},{17,25,21,19,17,17,17},
{14,17,17,17,17,17,14},{30,17,17,30,16,16,16},
{14,17,17,17,21,18,13},{30,17,17,30,20,18,17},
{15,16,16,14,1,1,30},{31,4,4,4,4,4,4},
{17,17,17,17,17,17,14},{17,17,17,17,17,10,4},
{17,17,17,21,21,21,10},{17,17,10,4,10,17,17},
{17,17,10,4,4,4,4},{31,1,2,4,8,16,31},
{14,17,19,21,25,17,14},{4,12,4,4,4,4,14},
{14,17,1,2,4,8,31},{30,1,1,14,1,1,30},
{2,6,10,18,31,2,2},{31,16,16,30,1,1,30},
{14,16,16,30,17,17,14},{31,1,2,4,8,8,8},
{14,17,17,14,17,17,14},{14,17,17,15,1,1,14}
};

static void rect(float x,float y,float w,float h,Color c) {
    if(w<=0 || h<=0) return;

    Vtx *v=(Vtx*)sceGuGetMemory(2*sizeof(Vtx));
    v[0]=(Vtx){c,x,y,0};
    v[1]=(Vtx){c,x+w,y+h,0};

    sceGuDrawArray(
        GU_SPRITES,
        GU_COLOR_8888|GU_VERTEX_32BITF|GU_TRANSFORM_2D,
        2,0,v
    );
}

static void wr(float x,float y,float w,float h,Color c) {
    rect(x-cam,y,w,h,c);
}

static void text5(int x,int y,const char *s,Color c,int scale) {
    for(;*s;s++,x+=6*scale) {
        char ch=*s;
        int idx=-1;

        if(ch>='A'&&ch<='Z') idx=ch-'A';
        else if(ch>='0'&&ch<='9') idx=26+ch-'0';

        if(ch=='-') {
            rect(x,y+3*scale,4*scale,scale,c);
            continue;
        }

        if(ch==':') {
            rect(x+2*scale,y+2*scale,scale,scale,c);
            rect(x+2*scale,y+5*scale,scale,scale,c);
            continue;
        }

        if(idx<0) continue;

        for(int r=0;r<7;r++) {
            for(int b=0;b<5;b++) {
                if(font5x7[idx][r]&(1<<(4-b))) {
                    rect(x+b*scale,y+r*scale,scale,scale,c);
                }
            }
        }
    }
}

static Box playerBox(void) {
    return (Box){px,py,PW,PH};
}

static int hit(Box a,Box b) {
    return a.x<b.x+b.w && a.x+a.w>b.x &&
           a.y<b.y+b.h && a.y+a.h>b.y;
}

static void addXP(int n) {
    xp+=n;

    while(xp>=nextXP) {
        xp-=nextXP;
        level++;
        nextXP+=10;
        if(hp<maxHP) hp++;
    }
}

static void drawMenu(void) {
    rect(0,0,SW,SH,BLACK);
    rect(18,18,444,236,DARK);
    rect(24,24,432,224,BLACK);

    text5(135,42,"RANDOM ALPHA",WHITE,3);
    text5(152,77,"CAVERNA",GRAY,2);

    const char *opts[3]={"JOGAR","CONTINUAR","SAIR"};

    for(int i=0;i<3;i++) {
        if(i==menuChoice)
            rect(130,119+i*27,220,20,STONE);

        text5(151,125+i*27,opts[i],
              i==menuChoice?WHITE:GRAY,2);
    }

    text5(60,218,"X SELECIONA SETAS MOVEM",LIGHT,1);
}

static void drawDialog(void) {
    rect(18,178,444,78,BLACK);
    rect(18,178,444,2,WHITE);
    rect(18,254,444,2,WHITE);

    if(dialogStep==0) {
        text5(30,190,"VOCE ENTROU NA CAVERNA",WHITE,1);
        text5(30,204,"ENCONTRE O CRISTAL ROXO",LIGHT,1);
    } else if(dialogStep==1) {
        text5(30,190,"VOCE DESPERTOU A MAGIA",WHITE,1);
        text5(30,204,"CIRCULO LANCAR MAGIA",LIGHT,1);
    } else {
        text5(30,190,"O GUARDIAO BLOQUEIA A SAIDA",WHITE,1);
        text5(30,204,"DERROTE O CHEFE DA CAVERNA",LIGHT,1);
    }

    text5(340,235,"X CONTINUAR",YELLOW,1);
}

static void drawPlayer(void) {
    float x=px-cam;

    /* Personagem provisório desenhado em preto e branco */
    rect(x+4,py+2,10,12,WHITE);
    rect(x+2,py+13,14,13,GRAY);
    rect(x,py+25,7,5,WHITE);
    rect(x+11,py+25,7,5,WHITE);

    /* Espada */
    rect(x+(face?13:-7),py+15,12,3,WHITE);
}

static void drawScene(void) {
    rect(0,0,SW,SH,BLACK);

    /* Teto da caverna */
    wr(0,0,WW,GROUND,DARK);

    for(int i=0;i<25;i++) {
        float x=i*65.0f;
        rect(x-cam,0,8,12+(i%4)*5,GRAY);
    }

    /* Piso */
    wr(0,GROUND,WW,22,GRAY);
    wr(0,GROUND+22,WW,20,BLACK);

    /* Plataformas e paredes */
    wr(190,190,100,10,WHITE);
    wr(310,145,12,105,GRAY);
    wr(430,160,110,10,WHITE);
    wr(555,120,12,130,GRAY);
    wr(650,190,100,10,WHITE);
    wr(680,135,70,10,GRAY);
    wr(850,180,100,10,WHITE);
    wr(930,150,12,100,GRAY);
    wr(1080,155,100,10,WHITE);
    wr(1170,175,12,75,GRAY);
    wr(1300,235,150,15,WHITE);

    /* Cristal que libera magia */
    wr(713,107,24,10,PURPLE);
    wr(720,100,10,24,PURPLE);
    wr(718,102,14,20,WHITE);

    /* Porta da arena do chefe */
    wr(1450,140,12,110,GRAY);
    wr(1470,140,12,110,GRAY);
    wr(1450,140,32,8,WHITE);

    /* Chefe */
    if(bossActive) {
        wr(1350,215,36,32,WHITE);
        wr(1355,222,7,6,BLACK);
        wr(1373,222,7,6,BLACK);

        rect(335,8,135,8,DARK);
        rect(337,10,131.0f*bossHP/20.0f,4,WHITE);
        text5(335,20,"GUARDIAO",WHITE,1);
    }

    drawPlayer();

    /* HUD */
    text5(8,8,"VIDA",WHITE,1);
    rect(8,20,80,5,DARK);
    rect(8,20,80.0f*hp/(maxHP?maxHP:1),5,WHITE);

    text5(8,32,"XP",WHITE,1);
    rect(8,43,80,4,DARK);
    rect(8,43,80.0f*xp/(nextXP?nextXP:1),4,GRAY);

    text5(100,8,"NIVEL",WHITE,1);

    if(magic)
        text5(100,22,"MAGIA LIBERADA",WHITE,1);

    if(px>1180 && bossActive)
        text5(130,55,"GUARDIAO DA CAVERNA",WHITE,1);

    if(gameMode==DIALOG) drawDialog();

    if(gameMode==PAUSE) {
        rect(100,70,280,120,BLACK);
        rect(100,70,280,3,WHITE);
        text5(175,88,"PAUSADO",WHITE,2);
        text5(125,120,"START CONTINUAR",WHITE,1);
        text5(125,140,"SELECT RENASCER",WHITE,1);
    }

    if(gameMode==WIN) {
        rect(30,70,420,130,BLACK);
        text5(125,92,"GUARDIAO DERROTADO",WHITE,2);
        text5(105,125,"FIM DO PROTOTIPO",WHITE,1);
        text5(125,150,"START MENU",WHITE,1);
    }
}

static void resetGame(void) {
    px=60;
    py=GROUND-PH;
    vy=0;
    cam=0;
    hp=maxHP;
    xp=0;
    level=1;
    nextXP=20;
    magic=0;
    magicCD=0;
    swordCD=0;
    bossHP=20;
    bossActive=1;
    bossStarted=0;
    jumps=2;
    gameMode=DIALOG;
    dialogStep=0;
}

static void updateGame(SceCtrlData p,unsigned int old) {
    float sp=2.0f;

    int left=(p.Buttons&PSP_CTRL_LEFT)||(p.Lx<80);
    int right=(p.Buttons&PSP_CTRL_RIGHT)||(p.Lx>175);

    if(left) {
        px-=sp;
        face=0;
    }

    if(right) {
        px+=sp;
        face=1;
    }

    if(px<0) px=0;
    if(px>WW-PW) px=WW-PW;

    if(pressed(p.Buttons,old,PSP_CTRL_CROSS)&&jumps>0) {
        vy=-6.3f;
        jumps--;
    }

    vy+=0.34f;
    py+=vy;

    if(py+PH>=GROUND) {
        py=GROUND-PH;
        vy=0;
        jumps=2;
    }

    if(magicCD>0) magicCD--;
    if(swordCD>0) swordCD--;

    /* Pegar cristal */
    if(!magic && hit(playerBox(),(Box){713,100,24,24})) {
        magic=1;
        gameMode=DIALOG;
        dialogStep=1;
    }

    /* Entrada na arena */
    if(px>1180 && !bossStarted) {
        bossStarted=1;
        gameMode=DIALOG;
        dialogStep=2;
    }

    /* Movimento simples do chefe */
    if(bossActive) {
        if(fabsf(px-1350)<250 && bossHP>0) {
            static int bossTimer=0;
            bossTimer++;

            if(bossTimer>90) {
                bossTimer=0;
                hp--;
            }
        }

        if(hp<=0) {
            hp=maxHP;
            px=60;
            py=GROUND-PH;
            cam=0;
        }
    }

    if(!bossActive && bossStarted)
        gameMode=WIN;

    cam=px-SW/2;
    if(cam<0) cam=0;
    if(cam>WW-SW) cam=WW-SW;
}

int main(void) {
    sceCtrlSetSamplingCycle(0);
    sceCtrlSetSamplingMode(PSP_CTRL_MODE_ANALOG);

    sceGuInit();
    sceGuStart(GU_DIRECT,list);

    sceGuDrawBuffer(GU_PSM_8888,(void*)0x00000,512);
    sceGuDispBuffer(SW,SH,(void*)0x88000,512);
    sceGuDepthBuffer((void*)0x110000,512);

    sceGuOffset(2048-SW/2,2048-SH/2);
    sceGuViewport(2048,2048,SW,SH);
    sceGuDepthRange(65535,0);
    sceGuScissor(0,0,SW,SH);
    sceGuEnable(GU_SCISSOR_TEST);
    sceGuDisable(GU_DEPTH_TEST);
    sceGuDisable(GU_TEXTURE_2D);

    sceGuFinish();
    sceGuSync(0,0);
    sceDisplayWaitVblankStart();
    sceGuDisplay(GU_TRUE);

    SceCtrlData p;
    unsigned int old=0;

    while(1) {
        sceCtrlReadBufferPositive(&p,1);

        if(gameMode==MENU) {
            if(pressed(p.Buttons,old,PSP_CTRL_UP))
                menuChoice=(menuChoice+2)%3;

            if(pressed(p.Buttons,old,PSP_CTRL_DOWN))
                menuChoice=(menuChoice+1)%3;

            if(pressed(p.Buttons,old,PSP_CTRL_CROSS)) {
                if(menuChoice==2) {
                    menuChoice=0;
                } else {
                    resetGame();
                }
            }
        } else if(gameMode==DIALOG) {
            if(pressed(p.Buttons,old,PSP_CTRL_CROSS) ||
               pressed(p.Buttons,old,PSP_CTRL_CIRCLE)) {
                if(dialogStep==0 || dialogStep==1) {
                    gameMode=GAME;
                } else {
                    gameMode=GAME;
                }
            }
        } else if(gameMode==PAUSE) {
            if(pressed(p.Buttons,old,PSP_CTRL_START))
                gameMode=GAME;

            if(pressed(p.Buttons,old,PSP_CTRL_SELECT)) {
                px=60;
                py=GROUND-PH;
                hp=maxHP;
                cam=0;
                gameMode=GAME;
            }
        } else if(gameMode==WIN) {
            if(pressed(p.Buttons,old,PSP_CTRL_START)) {
                gameMode=MENU;
                menuChoice=0;
            }
        } else {
            if(pressed(p.Buttons,old,PSP_CTRL_START)) {
                gameMode=PAUSE;
            } else {
                if(pressed(p.Buttons,old,PSP_CTRL_CIRCLE) &&
                   magic && magicCD==0) {
                    /* Magia causa dano ao chefe */
                    if(bossActive && px>1250 && px<1430) {
                        bossHP-=3;
                        if(bossHP<=0) {
                            bossHP=0;
                            bossActive=0;
                            addXP(50);
                            gameMode=WIN;
                        }
                    }
                    magicCD=25;
                }

                if((p.Buttons&PSP_CTRL_SQUARE) && swordCD==0) {
                    if(bossActive && px>1310 && px<1410) {
                        bossHP-=2;
                        if(bossHP<=0) {
                            bossHP=0;
                            bossActive=0;
                            addXP(50);
                            gameMode=WIN;
                        }
                    }
                    swordCD=15;
                }

                updateGame(p,old);
            }
        }

        sceGuStart(GU_DIRECT,list);
        sceGuClearColor(BLACK);
        sceGuClear(GU_COLOR_BUFFER_BIT|GU_DEPTH_BUFFER_BIT);

        if(gameMode==MENU) {
            drawMenu();
        } else {
            drawScene();
        }

        sceGuFinish();
        sceGuSync(0,0);
        sceDisplayWaitVblankStart();
        sceGuSwapBuffers();

        old=p.Buttons;
        sceKernelDelayThread(16667);
    }

    sceGuTerm();
    sceKernelExitGame();
    return 0;
}
