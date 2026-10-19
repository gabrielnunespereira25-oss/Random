#include <pspkernel.h>
#include <pspdisplay.h>
#include <pspctrl.h>
#include <pspgu.h>
#include <math.h>

PSP_MODULE_INFO("RANDOM_ALPHA_CHAR", 0, 1, 0);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER);

#define SW 480
#define SH 272
#define FLOOR_Y 226.0f
#define GRAVITY 0.42f
#define JUMP_V -7.8f
#define MOVE_SPEED 2.2f

typedef unsigned int Color;
typedef struct { Color color; float x,y,z; } Vertex;

static unsigned int __attribute__((aligned(16))) list[262144];
static unsigned int oldButtons=0;
static float px=240, py=FLOOR_Y, vx=0, vy=0, anim=0;
static int facing=1, grounded=1, running=0;

#define BLACK 0xFF080808
#define DARK  0xFF242424
#define MID   0xFF777777
#define LIGHT 0xFFBBBBBB
#define WHITE 0xFFFFFFFF

static int pressed(unsigned int n,unsigned int o,unsigned int b) {
    return (n&b)&&!(o&b);
}

static void rect(float x,float y,float w,float h,Color c) {
    Vertex *v=(Vertex*)sceGuGetMemory(2*sizeof(Vertex));
    v[0]=(Vertex){c,x,y,0};
    v[1]=(Vertex){c,x+w,y+h,0};
    sceGuDrawArray(GU_SPRITES,
        GU_COLOR_8888|GU_VERTEX_32BITF|GU_TRANSFORM_2D,2,0,v);
}

static void tri(float x1,float y1,float x2,float y2,
                float x3,float y3,Color c) {
    Vertex *v=(Vertex*)sceGuGetMemory(3*sizeof(Vertex));
    v[0]=(Vertex){c,x1,y1,0};
    v[1]=(Vertex){c,x2,y2,0};
    v[2]=(Vertex){c,x3,y3,0};
    sceGuDrawArray(GU_TRIANGLES,
        GU_COLOR_8888|GU_VERTEX_32BITF|GU_TRANSFORM_2D,3,0,v);
}

static void line(float x1,float y1,float x2,float y2,
                 float thick,Color c) {
    float dx=x2-x1,dy=y2-y1;
    float len=sqrtf(dx*dx+dy*dy);
    if(len<0.01f) return;
    float ox=-dy/len*thick*0.5f;
    float oy= dx/len*thick*0.5f;
    tri(x1+ox,y1+oy,x2+ox,y2+oy,x2-ox,y2-oy,c);
    tri(x1+ox,y1+oy,x2-ox,y2-oy,x1-ox,y1-oy,c);
}

static void limb(float x1,float y1,float x2,float y2,float thick) {
    line(x1,y1,x2,y2,thick+2,WHITE);
    line(x1,y1,x2,y2,thick,DARK);
    rect(x2-thick*.35f,y2-thick*.35f,
         thick*.7f,thick*.7f,MID);
}

static void flame(float x,float y) {
    tri(x-9,y+3,x-8,y-8,x-2,y-15,WHITE);
    tri(x-2,y-15,x+1,y-5,x+8,y-12,WHITE);
    tri(x+8,y-12,x+10,y-2,x+7,y+5,WHITE);
    tri(x-8,y-1,x-4,y-10,x+1,y-3,LIGHT);
    tri(x+1,y-3,x+5,y-8,x+6,y+3,MID);
    tri(x-4,y+2,x-2,y-5,x+3,y+2,DARK);
}

static void character(void) {
    float bob=0,stride=0,arm=0;
    float x=px,feet=py;

    if(grounded&&running) {
        bob=fabsf(sinf(anim*2))*2.2f;
        stride=sinf(anim*2)*10;
        arm=-sinf(anim*2)*8;
    } else if(grounded) {
        bob=sinf(anim*.7f)*1.2f;
        arm=sinf(anim*.7f)*1.5f;
    } else {
        stride=(vy<0)?-4:5;
        arm=(vy<0)?-10:8;
    }

    rect(x-13,FLOOR_Y+1,26,2,MID);

    float hip=feet-22+bob;
    float kLX=x-5+stride*.45f;
    float kLY=feet-12-fabsf(stride)*.12f;
    float kRX=x+5-stride*.45f;
    float kRY=feet-12-fabsf(stride)*.12f;

    if(!grounded) {
        kLX=x-8; kLY=feet-13;
        kRX=x+8; kRY=feet-16;
    }

    limb(x-4,hip,kLX,kLY,5);
    limb(kLX,kLY,x-7+stride,feet-2,5);
    limb(x+4,hip,kRX,kRY,5);
    limb(kRX,kRY,x+8-stride,feet-2,5);

    tri(x-13+stride,feet-5,x-5+stride,feet-5,
        x-4+stride,feet-1,WHITE);
    tri(x+3-stride,feet-5,x+11-stride,feet-5,
        x+13-stride,feet-1,WHITE);

    float shoulder=feet-42+bob;
    float backX=x-10-facing*2+arm*.4f;
    float backY=shoulder+9;
    float handX=x-12+arm*.65f;
    float handY=shoulder+18;

    limb(x-6,shoulder,backX,backY,5);
    limb(backX,backY,handX,handY,4);

    float elbowX=x+facing*(10+arm*.35f);
    float elbowY=shoulder+8-arm*.25f;
    float frontX=x+facing*(13+arm*.7f);
    float frontY=shoulder+17-arm*.4f;

    limb(x+6,shoulder,elbowX,elbowY,5);
    limb(elbowX,elbowY,frontX,frontY,4);

    tri(x-9,feet-48+bob,x+8,feet-48+bob,
        x+10,feet-27+bob,WHITE);
    tri(x-9,feet-48+bob,x+10,feet-27+bob,
        x-7,feet-25+bob,WHITE);

    tri(x-6,feet-44+bob,x+5,feet-44+bob,
        x+7,feet-29+bob,DARK);
    tri(x-6,feet-44+bob,x+7,feet-29+bob,
        x-5,feet-28+bob,DARK);

    line(x-6,feet-40+bob,x+6,feet-32+bob,2,LIGHT);
    rect(x-3,feet-27+bob,7,4,MID);

    rect(x-3,feet-53+bob,6,7,WHITE);
    flame(x,feet-59+bob);

    if(facing>0) rect(x+1,feet-61+bob,4,2,BLACK);
    else rect(x-5,feet-61+bob,4,2,BLACK);

    if(running&&grounded) {
        int p=(int)(anim*4)%3;
        rect(x-facing*(15+p*3),feet-51-p*3,2,2,
             p==1?MID:WHITE);
    }
}

static void update(SceCtrlData *pad) {
    running=0;
    vx=0;

    if(pad->Buttons&PSP_CTRL_LEFT) {
        vx=-MOVE_SPEED;
        facing=-1;
        running=1;
    }
    if(pad->Buttons&PSP_CTRL_RIGHT) {
        vx=MOVE_SPEED;
        facing=1;
        running=1;
    }

    px+=vx;
    if(px<24) px=24;
    if(px>SW-24) px=SW-24;

    if(pressed(pad->Buttons,oldButtons,PSP_CTRL_CROSS)&&grounded) {
        vy=JUMP_V;
        grounded=0;
    }

    if(!grounded) {
        vy+=GRAVITY;
        py+=vy;
        if(py>=FLOOR_Y) {
            py=FLOOR_Y;
            vy=0;
            grounded=1;
        }
    }

    anim+=(running||!grounded)?0.19f:0.035f;
}

int main(void) {
    SceCtrlData pad;
    sceCtrlSetSamplingCycle(0);
    sceCtrlSetSamplingMode(PSP_CTRL_MODE_ANALOG);

    sceGuInit();
    sceGuStart(GU_DIRECT,list);
    sceGuDrawBuffer(GU_PSM_8888,(void*)0,512);
    sceGuDispBuffer(SW,SH,(void*)0x88000,512);
    sceGuDepthBuffer((void*)0x110000,512);
    sceGuOffset(2048-SW/2,2048-SH/2);
    sceGuViewport(2048,2048,SW,SH);
    sceGuScissor(0,0,SW,SH);
    sceGuEnable(GU_SCISSOR_TEST);
    sceGuDisable(GU_DEPTH_TEST);
    sceGuDisable(GU_TEXTURE_2D);
    sceGuFinish();
    sceGuSync(0,0);
    sceDisplayWaitVblankStart();
    sceGuDisplay(GU_TRUE);

    while(1) {
        sceCtrlReadBufferPositive(&pad,1);
        if(pressed(pad.Buttons,oldButtons,PSP_CTRL_START)) break;

        update(&pad);

        sceGuStart(GU_DIRECT,list);
        sceGuClearColor(BLACK);
        sceGuClear(GU_COLOR_BUFFER_BIT|GU_DEPTH_BUFFER_BIT);
        sceGuDisable(GU_DEPTH_TEST);
        sceGuDisable(GU_TEXTURE_2D);

        rect(0,FLOOR_Y+3,SW,2,MID);
        for(int i=0;i<SW;i+=24)
            rect(i,FLOOR_Y+7,12,1,DARK);

        character();

        sceGuFinish();
        sceGuSync(0,0);
        sceDisplayWaitVblankStart();
        sceGuSwapBuffers();

        oldButtons=pad.Buttons;
        sceKernelDelayThread(16667);
    }

    sceGuDisplay(GU_FALSE);
    sceGuTerm();
    sceKernelExitGame();
    return 0;
}
