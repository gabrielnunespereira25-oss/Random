
#include <pspkernel.h>
#include <pspdisplay.h>
#include <pspctrl.h>
#include <pspgu.h>
#include <pspge.h>
#include <math.h>

PSP_MODULE_INFO("RANDOM", 0, 1, 0);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER);

#define W 480
#define H 272
#define STRIDE 512
#define PI 3.14159265f

static unsigned int __attribute__((aligned(16))) list[262144];

typedef struct {
    unsigned int color;
    float x, y, z;
} V;

static float px = 70, py = 195, vy = 0;
static float camera = 0;
static float anim = 0;
static float spiderX = 330;
static float crystalPulse = 0;
static int grounded = 1;
static int face = 1;
static int hp = 5;
static int attack = 0;
static int magic = 0;
static int invuln = 0;
static int spiderAlive = 1;

static unsigned int C(int r, int g, int b) {
    return GU_RGBA(r, g, b, 255);
}

static void tri(float x1,float y1,float x2,float y2,
                float x3,float y3,unsigned int c) {
    V v[3] = {
        {c,x1,y1,0},
        {c,x2,y2,0},
        {c,x3,y3,0}
    };
    sceGuColor(0xFFFFFFFF);
    sceGuDrawArray(GU_TRIANGLES,
        GU_COLOR_8888 | GU_VERTEX_32BITF |
        GU_TRANSFORM_2D, 3, 0, v);
}

static void rect(float x,float y,float w,float h,
                 unsigned int c) {
    V v[2] = {
        {c,x,y,0},
        {c,x+w,y+h,0}
    };
    sceGuColor(0xFFFFFFFF);
    sceGuDrawArray(GU_SPRITES,
        GU_COLOR_8888 | GU_VERTEX_32BITF |
        GU_TRANSFORM_2D, 2, 0, v);
}

static void circ(float x,float y,float r,unsigned int c) {
    int i;
    for(i=0;i<16;i++) {
        float a=i*2*PI/16.0f;
        float b=(i+1)*2*PI/16.0f;
        tri(x,y,
            x+cosf(a)*r,y+sinf(a)*r,
            x+cosf(b)*r,y+sinf(b)*r,c);
    }
}

static void ln(float x1,float y1,float x2,float y2,
               unsigned int c) {
    V v[2] = {
        {c,x1,y1,0},
        {c,x2,y2,0}
    };
    sceGuColor(0xFFFFFFFF);
    sceGuDrawArray(GU_LINES,
        GU_COLOR_8888 | GU_VERTEX_32BITF |
        GU_TRANSFORM_2D, 2, 0, v);
}

/* Fundo profundo da caverna */
static void background(void) {
    int i;
    rect(0,0,W,H,C(10,12,27));

    /* Luz distante */
    circ(240,110,105,C(17,24,46));
    circ(240,110,72,C(20,29,49));

    /* Silhuetas das paredes */
    for(i=0;i<9;i++) {
        float x=i*65-fmodf(camera*0.18f,65);
        tri(x,0,x+58,0,x+28,75+(i%3)*20,
            C(24,27,45));
        tri(x+20,0,x+65,0,x+45,43+(i%4)*12,
            C(31,32,51));
    }

    /* Estalactites */
    for(i=0;i<13;i++) {
        float x=i*43-fmodf(camera*0.32f,43);
        float len=14+(i%4)*8;
        tri(x,0,x+16,0,x+8,len,C(42,43,65));
        tri(x+3,0,x+13,0,x+8,len-5,C(57,52,74));
    }

    /* Cristais distantes */
    for(i=0;i<7;i++) {
        float x=i*91-fmodf(camera*0.4f,91);
        tri(x,160,x+8,138,x+14,160,C(35,83,115));
        tri(x+8,138,x+18,158,x+14,160,C(49,115,143));
    }

    /* Paredes laterais e chão */
    rect(0,235,W,37,C(34,31,43));
    rect(0,235,W,5,C(61,91,65));
    rect(0,240,W,3,C(42,65,52));

    for(i=0;i<18;i++) {
        float x=i*31-fmodf(camera,31);
        tri(x,235,x+9,235,x+4,241,C(88,118,71));
        rect(x+5,249,14,3,C(49,45,57));
        rect(x+17,260,8,3,C(43,40,52));
    }

    /* Musgo e pequenas plantas */
    for(i=0;i<12;i++) {
        float x=i*47-fmodf(camera*0.8f,47);
        ln(x,235,x-3,227,C(53,108,65));
        ln(x,235,x+4,229,C(66,130,73));
        circ(x,231,2,C(76,143,78));
    }
}

/* Cristal mágico */
static void crystal(float x,float y) {
    float p=sinf(crystalPulse)*2.5f;
    circ(x,y+5,15,C(19,39,78));
    circ(x,y+5,10,C(24,65,105));

    tri(x,y-15+p,x-8,y+1,x,y+10,C(51,151,231));
    tri(x,y-15+p,x+8,y+1,x,y+10,C(83,211,255));
    tri(x,y-15+p,x,y-5,x-2,y+1,C(190,245,255));
    tri(x,y+10,x-4,y+1,x-8,y+1,C(36,105,180));
    ln(x-11,y+4,x-15,y+8,C(79,177,255));
    ln(x+11,y-2,x+15,y-7,C(79,177,255));
}

/* Plataformas com textura */
static void platform(float x,float y,float w) {
    rect(x,y,w,9,C(48,49,57));
    rect(x,y,w,3,C(100,137,83));
    rect(x+3,y+4,w-6,2,C(66,70,72));
    ln(x+10,y+4,x+20,y+4,C(92,91,91));
    ln(x+w-20,y+5,x+w-8,y+5,C(76,78,75));
}

/* Aranha de grama */
static void spider(float x,float y) {
    int i;
    float bob=sinf(anim*2.0f)*1.5f;
    y+=bob;

    /* Sombra */
    circ(x,y+11,14,C(17,22,23));

    /* Patas */
    for(i=0;i<4;i++) {
        float a=x-8+i*5;
        float kick=sinf(anim*5+i)*2;
        ln(a,y+2,a-9,y+10+kick,C(36,92,43));
        ln(a-9,y+10+kick,a-13,y+12+kick,C(46,115,49));
        ln(a+5,y+2,a+13,y+9-kick,C(36,92,43));
        ln(a+13,y+9-kick,a+16,y+12-kick,C(46,115,49));
    }

    /* Corpo */
    circ(x,y,11,C(28,91,43));
    circ(x,y-2,8,C(47,145,55));
    circ(x-3,y-5,3,C(78,185,69));
    circ(x+3,y-5,3,C(57,164,60));

    /* Olhos */
    circ(x-4,y-1,2.4f,C(210,25,37));
    circ(x+4,y-1,2.4f,C(210,25,37));
    circ(x-4.5f,y-1.5f,0.8f,C(255,220,200));
    circ(x+3.5f,y-1.5f,0.8f,C(255,220,200));

    /* Folhinhas */
    tri(x-5,y-8,x-12,y-15,x-3,y-12,C(48,132,58));
    tri(x+3,y-8,x+11,y-15,x+5,y-11,C(65,156,64));
}

/* Protagonista */
static void player(float x,float y) {
    float step=grounded?sinf(anim*9)*2:0;
    float flame=sinf(anim*12)*2;

    /* Sombra */
    circ(x+13,y+40,13,C(17,20,30));

    /* Perna única */
    rect(x+9,y+28+step,10,12,C(31,29,39));
    rect(x+10,y+30+step,7,5,C(49,43,52));
    rect(x+9,y+39+step,13,3,C(24,23,33));

    /* Corpo triangular */
    tri(x+2,y+10,x+26,y+10,x+14,y+32,
        C(22,22,32));
    tri(x+5,y+12,x+23,y+12,x+14,y+27,
        C(44,35,49));
    ln(x+7,y+15,x+14,y+27,C(77,44,54));

    /* Cabeça */
    circ(x+14,y+7,8,C(211,161,132));
    circ(x+14,y+8,6,C(228,183,146));

    /* Cabelo de fogo feito de círculos */
    circ(x+6,y+2,5,C(197,42,18));
    circ(x+10,y-2+flame,6,C(255,78,15));
    circ(x+17,y-3-flame*0.5f,6,C(255,118,12));
    circ(x+22,y+2,4,C(220,44,16));
    circ(x+13,y+2,4,C(255,192,27));
    circ(x+10,y-2+flame,2,C(255,180,35));
    circ(x+18,y-4-flame*0.5f,2,C(255,218,65));

    /* Olho */
    circ(x+(face>0?18:9),y+7,1.5f,C(185,24,35));

    /* Lenço escuro */
    rect(x+6,y+12,15,3,C(75,28,43));
    tri(x+8,y+14,x+3,y+22,x+11,y+18,C(107,30,45));

    /* Espada */
    if(attack>0) {
        float sx=x+(face>0?22:3);
        float sy=y+20;
        float ex=sx+face*22;
        float ey=sy-15;

        ln(sx,sy,ex,ey,C(255,223,160));
        ln(sx+1,sy,ex,ey,C(226,240,255));
        ln(sx-face*3,sy+3,sx+face*4,sy-4,
           C(180,113,66));

        /* Arco de ataque */
        ln(ex,ey,ex+face*3,ey-3,C(160,210,255));
        ln(ex-face*2,ey+4,ex+face*2,ey+1,
           C(120,175,255));
    }
}

/* HUD */
static void hud(void) {
    int i;

    rect(9,9,105,19,C(6,9,19));
    rect(11,11,101,15,C(54,27,43));
    rect(12,12,hp*19,13,C(190,38,58));
    rect(12,12,hp*19,3,C(255,90,91));

    for(i=0;i<5;i++) {
        circ(17+i*19,18,2,C(255,151,143));
    }

    /* Indicador de magia */
    circ(133,18,9,C(20,36,66));
    circ(133,18,6,C(45,113,193));
    circ(131,16,2,C(166,229,255));

    rect(352,9,119,19,C(6,9,19));
    rect(355,12,113,13,C(25,29,48));
    rect(359,15,5,7,C(255,83,25));
    rect(367,15,5,7,C(255,153,24));
    rect(375,15,5,7,C(255,61,23));
    rect(388,14,72,2,C(68,78,102));
    rect(388,18,45,2,C(68,78,102));
}

/* Efeito mágico */
static void magicEffect(float x,float y) {
    int i;
    float r=8+fmodf(anim*8,9);

    circ(x,y,r+5,C(21,35,91));
    circ(x,y,r,C(44,80,180));
    circ(x,y,r*0.55f,C(88,165,255));
    circ(x-2,y-2,3,C(208,243,255));

    for(i=0;i<5;i++) {
        float a=anim*4+i*2*PI/5;
        float sx=x+cosf(a)*r;
        float sy=y+sinf(a)*r;
        circ(sx,sy,1.5f,C(140,210,255));
    }
}

static void initGraphics(void) {
    sceGuInit();
    sceGuStart(GU_DIRECT,list);

    sceGuDrawBuffer(GU_PSM_8888,(void*)0,STRIDE);
    sceGuDispBuffer(W,H,(void*)0x88000,STRIDE);
    sceGuDepthBuffer((void*)0x110000,STRIDE);

    sceGuOffset(2048-(W/2),2048-(H/2));
    sceGuViewport(2048,2048,W,H);
    sceGuDepthRange(65535,0);

    sceGuScissor(0,0,W,H);
    sceGuEnable(GU_SCISSOR_TEST);
    sceGuDisable(GU_DEPTH_TEST);
    sceGuDisable(GU_CULL_FACE);
    sceGuDisable(GU_TEXTURE_2D);
    sceGuShadeModel(GU_FLAT);

    sceGuFinish();
    sceGuSync(0,0);
    sceDisplayWaitVblankStart();
    sceGuDisplay(GU_TRUE);
}

static void update(void) {
    SceCtrlData p;
    sceCtrlReadBufferPositive(&p,1);

    anim+=0.08f;
    crystalPulse+=0.09f;

    if(invuln>0) invuln--;

    if(p.Buttons & PSP_CTRL_LEFT) {
        px-=2.0f;
        face=-1;
    }
    if(p.Buttons & PSP_CTRL_RIGHT) {
        px+=2.0f;
        face=1;
    }

    if(p.Buttons & PSP_CTRL_RTRIGGER) {
        if(p.Buttons & PSP_CTRL_LEFT) px-=1.5f;
        if(p.Buttons & PSP_CTRL_RIGHT) px+=1.5f;
    }

    if((p.Buttons & PSP_CTRL_CROSS) && grounded) {
        vy=-7.0f;
        grounded=0;
    }

    if(p.Buttons & PSP_CTRL_SQUARE) attack=8;
    if(p.Buttons & PSP_CTRL_TRIANGLE) magic=10;

    vy+=0.29f;
    py+=vy;

    if(py>=195) {
        py=195;
        vy=0;
        grounded=1;
    }

    if(px<12) px=12;
    if(px>1100) px=1100;

    camera=px-100;
    if(camera<0) camera=0;

    if(spiderAlive) {
        if(spiderX<px-4) spiderX+=0.55f;
        if(spiderX>px+4) spiderX-=0.55f;

        if(attack>0 && fabsf(px-spiderX)<48 &&
           fabsf(py-195)<38) {
            spiderAlive=0;
        } else if(invuln==0 &&
                  fabsf(px-spiderX)<22 &&
                  fabsf(py-195)<35) {
            if(hp>0) hp--;
            invuln=60;
            px-=face*12;
        }
    }

    if(attack>0) attack--;
    if(magic>0) magic--;
}

static void render(void) {
    float sx=px-camera;
    float ex=spiderX-camera;

    sceGuStart(GU_DIRECT,list);
    sceGuClearColor(C(10,12,27));
    sceGuClear(GU_COLOR_BUFFER_BIT|GU_DEPTH_BUFFER_BIT);

    background();

    /* Plataformas da caverna */
    platform(135-camera,196,62);
    platform(340-camera,177,69);
    platform(530-camera,202,82);
    platform(740-camera,187,70);

    /* Cristais do cenário */
    crystal(185-camera,219);
    crystal(402-camera,161);
    crystal(605-camera,218);
    crystal(820-camera,219);

    if(spiderAlive) spider(ex,220);

    /* Piscar após receber dano */
    if(invuln==0 || ((invuln/5)%2)==0)
        player(sx,py);

    if(magic>0)
        magicEffect(sx+face*(27+(10-magic)*5),py+17);

    hud();

    sceGuFinish();
    sceGuSync(0,0);
    sceDisplayWaitVblankStart();
    sceGuSwapBuffers();
}

int main(void) {
    SceCtrlData p;

    sceCtrlSetSamplingCycle(0);
    sceCtrlSetSamplingMode(PSP_CTRL_MODE_ANALOG);
    initGraphics();

    while(1) {
        sceCtrlReadBufferPositive(&p,1);
        if(p.Buttons & PSP_CTRL_START) break;

        update();
        render();
    }

    sceGuTerm();
    sceKernelExitGame();
    return 0;
}
