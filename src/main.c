/*
RANDOM - protótipo PSP
CONTROLES:
Analógico / direcional = mover
X = pular / pulo duplo
QUADRADO = espada
CÍRCULO = magia após pegar o cristal
TRIÂNGULO = gastar ponto de habilidade
L / R = selecionar habilidade
START = pausar
SELECT = voltar ao checkpoint
*/

#include <pspkernel.h>
#include <pspdisplay.h>
#include <pspctrl.h>
#include <pspgu.h>
#include <pspgum.h>
#include <math.h>

PSP_MODULE_INFO("RANDOM", 0, 1, 0);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER);

#define SW 480
#define SH 272
#define WW 1600
#define GROUND 250
#define PW 18
#define PH 30
#define CAVE 0
#define FOREST 1
#define N_MOBS 4

typedef unsigned int Color;
typedef struct { Color c; float x,y,z; } Vtx;
typedef struct { float x,y,w,h; } Box;

static unsigned int __attribute__((aligned(16))) list[262144];

#define BLACK  0xFF101018
#define DARK   0xFF252535
#define STONE  0xFF555568
#define GRAY   0xFF858596
#define LIGHT  0xFFBFC4D0
#define WHITE  0xFFFFFFFF
#define GREEN  0xFF35B85A
#define DGREEN 0xFF17633B
#define BLUE   0xFFFFA040
#define PURPLE 0xFFB050E8
#define RED    0xFF3030E8
#define YELLOW 0xFF30E8F8
#define BROWN  0xFF493020
#define SKIN   0xFF70B8E8

static int zone=CAVE, paused=0, face=1;
static int grounded=1, jumps=2;
static float cam=0, px=60, py=220, vy=0;

static int hp=5, maxHP=5, magic=0, magicCD=0;
static int xp=0, level=1, nextXP=20, points=0;
static int magicPower=1, swordDamage=1, speedLevel=1;
static int defense=0, selected=0;

static int cpZone=CAVE, cpReached[3]={0,0,0};
static float cpX=60, cpY=220;

typedef struct {
    float x,y,min,max,speed;
    int alive,hp,cd;
} Mob;

static Mob mobs[N_MOBS]={
    {300,220,250,380,.8f,1,1,0},
    {610,220,550,680,1,1,1,0},
    {960,220,900,1040,.9f,1,1,0},
    {1190,220,1150,1250,1.1f,1,1,0}
};

static struct {
    float x,y,vy;
    int grounded,timer,active,hp,cd;
} boss={1350,218,0,1,50,1,12,0};

typedef struct {
    int on;
    float x,y,vx;
} Shot;

static Shot shots[8];

static int pressed(unsigned int n,unsigned int o,unsigned int b){
    return (n&b)&&!(o&b);
}

static void rect(float x,float y,float w,float h,Color c){
    if(w<=0||h<=0)return;

    Vtx *v=(Vtx*)sceGuGetMemory(2*sizeof(Vtx));

    v[0]=(Vtx){c,x,y,0};
    v[1]=(Vtx){c,x+w,y+h,0};

    sceGuDrawArray(
        GU_SPRITES,
        GU_COLOR_8888|GU_VERTEX_32BITF|GU_TRANSFORM_2D,
        2,0,v
    );
}

static void wr(float x,float y,float w,float h,Color c){
    rect(x-cam,y,w,h,c);
}

static int hit(Box a,Box b){
    return a.x<b.x+b.w &&
           a.x+a.w>b.x &&
           a.y<b.y+b.h &&
           a.y+a.h>b.y;
}

static Box pbox(void){
    return (Box){px,py,PW,PH};
}

/* XP e níveis */

static void addXP(int n){
    xp+=n;

    while(xp>=nextXP){
        xp-=nextXP;
        level++;
        points++;
        nextXP+=10;

        if(hp<maxHP)hp++;
    }
}

/*
Habilidades:
0 = vida máxima
1 = poder mágico
2 = dano da espada
3 = velocidade
4 = defesa
*/

static void spend(void){
    if(!points)return;

    switch(selected){
        case 0:
            maxHP++;
            hp++;
            break;

        case 1:
            magicPower++;
            break;

        case 2:
            swordDamage++;
            break;

        case 3:
            speedLevel++;
            break;

        case 4:
            defense++;
            break;
    }

    points--;
}

static int damage(void){
    int d=2-defense;
    return d<1?1:d;
}

/* Checkpoint e renascimento */

static void respawn(void){
    zone=cpZone;
    px=cpX;
    py=cpY;
    vy=0;
    grounded=1;
    jumps=2;
    hp=maxHP;

    cam=px-SW/2;

    if(cam<0)cam=0;
    if(cam>WW-SW)cam=WW-SW;
}

/* Zona 1: caverna */

static void drawCave(void){
    rect(0,0,SW,SH,BLACK);

    wr(0,0,WW,GROUND,DARK);
    wr(0,GROUND,WW,22,STONE);
    wr(0,GROUND+22,WW,20,GRAY);
    wr(0,0,WW,24,STONE);

    for(int x=40;x<WW;x+=130){
        wr(x,24,14,18,GRAY);
        wr(x+4,42,6,10,STONE);
    }

    /* Plataformas */
    wr(190,190,100,10,STONE);
    wr(430,160,110,10,STONE);
    wr(650,190,100,10,STONE);
    wr(680,135,70,10,GRAY);
    wr(850,180,100,10,STONE);
    wr(1080,155,100,10,STONE);

    /* Arena do miniboss */
    wr(1300,235,150,15,GRAY);

    /* Cristal que desbloqueia magia */
    wr(713,107,24,10,PURPLE);
    wr(720,100,10,24,PURPLE);
    wr(718,102,14,20,WHITE);

    /* Passagens */
    wr(690,65,45,40,PURPLE);
    wr(1470,185,30,65,YELLOW);
}

/* Zona 2: floresta, sem mobs */

static void drawForest(void){
    rect(0,0,SW,SH,0xFF7DC6F0);

    wr(0,GROUND,WW,22,DGREEN);
    wr(0,GROUND+22,WW,20,BROWN);

    for(int x=80;x<WW;x+=170){
        wr(x,145,12,105,BROWN);
        wr(x-24,120,62,55,DGREEN);
        wr(x-15,95,45,45,GREEN);
    }

    wr(220,190,100,10,BROWN);
    wr(520,160,100,10,BROWN);
    wr(800,190,120,10,BROWN);
    wr(1080,150,100,10,BROWN);

    wr(1240,85,45,50,PURPLE);
    wr(120,185,25,65,YELLOW);
}

/* Personagem */

static void drawPlayer(void){
    float x=px-cam;

    rect(x+4,py+2,10,12,SKIN);
    rect(x+2,py+13,14,13,BLUE);
    rect(x,py+25,7,5,BROWN);
    rect(x+11,py+25,7,5,BROWN);

    rect(x+(face?13:-7),py+15,12,3,LIGHT);

    /* Barra de vida */
    rect(8,8,80,7,DARK);
    rect(9,9,78.0f*hp/(maxHP?maxHP:1),5,RED);
}

/* Mobs e miniboss: apenas na caverna */

static void drawMobs(void){
    if(zone!=CAVE)return;

    for(int i=0;i<N_MOBS;i++){
        if(!mobs[i].alive)continue;

        wr(mobs[i].x,mobs[i].y,20,25,RED);
        wr(mobs[i].x+4,mobs[i].y+5,4,4,WHITE);
        wr(mobs[i].x+13,mobs[i].y+5,4,4,WHITE);
    }

    if(boss.active){
        wr(boss.x,boss.y,36,32,PURPLE);
        wr(boss.x+5,boss.y+7,7,6,YELLOW);
        wr(boss.x+23,boss.y+7,7,6,YELLOW);

        /* Barra de vida do miniboss */
        rect(345,8,125,7,DARK);
        rect(346,9,123.0f*boss.hp/12.0f,5,RED);
    }
}

/* Checkpoints */

static void drawCheckpoints(void){
    float xs[3]={520,1050,800};
    int zs[3]={CAVE,CAVE,FOREST};

    for(int i=0;i<3;i++){
        if(zs[i]!=zone)continue;

        wr(xs[i],192,4,28,GRAY);
        wr(xs[i]+4,192,15,10,
           cpReached[i]?GREEN:YELLOW);
    }
}

/* HUD */

static void drawHUD(void){
    rect(8,20,80,5,DARK);
    rect(9,21,78.0f*xp/nextXP,3,YELLOW);

    rect(8,29,points?40:8,4,
         points?GREEN:GRAY);

    Color colors[5]={RED,PURPLE,YELLOW,GREEN,BLUE};

    for(int i=0;i<5;i++){
        rect(100+i*16,8,12,12,
             i==selected?colors[i]:GRAY);
    }

    if(magic)rect(184,8,12,12,PURPLE);

    if(paused){
        rect(130,75,220,110,DARK);
        rect(145,90,190,5,WHITE);
        rect(145,110,150,5,GRAY);
        rect(145,130,170,5,YELLOW);
        rect(145,150,120,5,GREEN);
    }
}

static void draw(void){
    sceGuStart(GU_DIRECT,list);

    sceGuClearColor(BLACK);
    sceGuClear(GU_COLOR_BUFFER_BIT|GU_DEPTH_BUFFER_BIT);

    sceGuDisable(GU_DEPTH_TEST);
    sceGuDisable(GU_TEXTURE_2D);

    if(zone==CAVE)
        drawCave();
    else
        drawForest();

    drawCheckpoints();
    drawMobs();

    for(int i=0;i<8;i++){
        if(shots[i].on)
            wr(shots[i].x,shots[i].y,9,7,PURPLE);
    }

    drawPlayer();
    drawHUD();

    sceGuFinish();
    sceGuSync(0,0);
    sceDisplayWaitVblankStart();
    sceGuSwapBuffers();
}

/* Magia */

static void cast(void){
    if(!magic||magicCD)return;

    for(int i=0;i<8;i++){
        if(!shots[i].on){
            shots[i]=(Shot){
                1,
                px+(face?PW:-8),
                py+12,
                face?5:-5
            };

            magicCD=18;
            break;
        }
    }
}

static void updateShots(void){
    for(int i=0;i<8;i++){
        if(!shots[i].on)continue;

        shots[i].x+=shots[i].vx;

        if(shots[i].x<0||shots[i].x>WW)
            shots[i].on=0;

        if(zone!=CAVE)continue;

        Box s={shots[i].x,shots[i].y,9,7};

        for(int m=0;m<N_MOBS;m++){
            if(!mobs[m].alive)continue;

            Box e={mobs[m].x,mobs[m].y,20,25};

            if(hit(s,e)){
                mobs[m].hp-=magicPower;
                shots[i].on=0;

                if(mobs[m].hp<=0){
                    mobs[m].alive=0;
                    addXP(5);
                }

                break;
            }
        }

        if(shots[i].on&&boss.active){
            Box e={boss.x,boss.y,36,32};

            if(hit(s,e)){
                boss.hp-=magicPower;
                shots[i].on=0;

                if(boss.hp<=0){
                    boss.active=0;
                    addXP(50);
                }
            }
        }
    }
}

/* Movimento e colisão dos inimigos */

static void updateMobs(void){
    if(zone!=CAVE)return;

    for(int i=0;i<N_MOBS;i++){
        if(!mobs[i].alive)continue;

        mobs[i].x+=mobs[i].speed;

        if(mobs[i].x<mobs[i].min||
           mobs[i].x>mobs[i].max)
            mobs[i].speed=-mobs[i].speed;

        if(mobs[i].cd>0)mobs[i].cd--;

        if(hit(pbox(),
               (Box){mobs[i].x,mobs[i].y,20,25})&&
           !mobs[i].cd){
            hp-=damage();
            mobs[i].cd=35;
        }
    }

    if(boss.active){
        if(boss.cd>0)boss.cd--;

        if(boss.grounded&&
           fabsf(px-boss.x)<300&&boss.timer<=0){
            boss.vy=-6;
            boss.grounded=0;
            boss.timer=70;
        }

        if(boss.timer>0)boss.timer--;

        boss.vy+=.25f;
        boss.y+=boss.vy;

        if(boss.y>=218){
            boss.y=218;
            boss.vy=0;
            boss.grounded=1;
        }

        if(px<boss.x)boss.x-=.5f;
        else boss.x+=.5f;

        if(boss.x<1300)boss.x=1300;
        if(boss.x>1410)boss.x=1410;

        if(hit(pbox(),
               (Box){boss.x,boss.y,36,32})&&!boss.cd){
            hp-=damage();
            boss.cd=45;
        }
    }
}

/* Espada */

static void sword(int down){
    static int was=0;

    if(!down){
        was=0;
        return;
    }

    if(was)return;
    was=1;

    if(zone!=CAVE)return;

    Box a={
        px+(face?PW:-24),
        py+5,
        24,
        20
    };

    for(int i=0;i<N_MOBS;i++){
        if(!mobs[i].alive)continue;

        if(hit(a,(Box){mobs[i].x,mobs[i].y,20,25})){
            mobs[i].hp-=swordDamage;

            if(mobs[i].hp<=0){
                mobs[i].alive=0;
                addXP(5);
            }
        }
    }

    if(boss.active&&!boss.cd&&
       hit(a,(Box){boss.x,boss.y,36,32})){
        boss.hp-=swordDamage;
        boss.cd=12;

        if(boss.hp<=0){
            boss.active=0;
            addXP(50);
        }
    }
}

/* Ativar checkpoints */

static void checkpoints(void){
    float xs[3]={520,1050,800};
    int zs[3]={CAVE,CAVE,FOREST};

    for(int i=0;i<3;i++){
        if(zs[i]!=zone)continue;

        if(hit(pbox(),(Box){xs[i],190,22,60})){
            cpZone=zone;
            cpX=xs[i]-10;
            cpY=220;
            cpReached[i]=1;
        }
    }
}

/* Passagens entre as zonas */

static void transition(void){
    if(zone==CAVE){
        if(px>=1470&&py>170){
            zone=FOREST;
            px=125;
            py=220;
        }
        else if(px>=690&&px<=735&&py<115){
            zone=FOREST;
            px=1270;
            py=115;
        }
    }
    else{
        if(px<=125&&py>170){
            zone=CAVE;
            px=1450;
            py=220;
        }
        else if(px>=1240&&px<=1290&&py<135){
            zone=CAVE;
            px=740;
            py=115;
        }
    }
}

/* Controles e física do jogador */

static void update(SceCtrlData p,unsigned int old){
    float sp=2.0f+.25f*(speedLevel-1);

    int l=(p.Buttons&PSP_CTRL_LEFT)||p.Lx<90;
    int r=(p.Buttons&PSP_CTRL_RIGHT)||p.Lx>165;

    if(l){
        px-=sp;
        face=0;
    }

    if(r){
        px+=sp;
        face=1;
    }

    if(px<0)px=0;
    if(px>WW-PW)px=WW-PW;

    if(pressed(p.Buttons,old,PSP_CTRL_CROSS)&&jumps>0){
        vy=-6.3f;
        grounded=0;
        jumps--;
    }

    vy+=.34f;
    py+=vy;

    if(py+PH>=GROUND){
        py=GROUND-PH;
        vy=0;
        grounded=1;
        jumps=2;
    }

    /* Colisão simples com plataformas */
    float xs[6]={190,430,650,680,850,1080};
    float ys[6]={190,160,190,135,180,155};
    float ws[6]={100,110,100,70,100,100};

    for(int i=0;i<6;i++){
        Box a=pbox();
        Box b={xs[i],ys[i],ws[i],10};

        if(vy>=0&&
           a.x+a.w>b.x&&a.x<b.x+b.w&&
           a.y+a.h>=b.y&&a.y+a.h<=b.y+12){
            py=b.y-PH;
            vy=0;
            grounded=1;
            jumps=2;
        }
    }

    if(magicCD>0)magicCD--;

    transition();
    checkpoints();
    updateMobs();
    updateShots();

    /* Pegar o cristal para liberar magia */
    if(zone==CAVE&&!magic&&
       hit(pbox(),(Box){713,100,24,24}))
        magic=1;

    if(hp<=0)respawn();

    cam=px-SW/2;

    if(cam<0)cam=0;
    if(cam>WW-SW)cam=WW-SW;
}

/* Inicialização PSP */

int main(void){
    sceCtrlSetSamplingCycle(0);
    sceCtrlSetSamplingMode(PSP_CTRL_MODE_ANALOG);

    sceGuInit();
    sceGuStart(GU_DIRECT,list);

    sceGuDrawBuffer(GU_PSM_8888,(void*)0,512);
    sceGuDispBuffer(SW,SH,(void*)0x44000000,512);
    sceGuDepthBuffer((void*)0x44088000,512);

    sceGuOffset(2048-SW/2,2048-SH/2);
    sceGuViewport(2048,2048,SW,SH);
    sceGuDepthRange(65535,0);
    sceGuScissor(0,0,SW,SH);
    sceGuEnable(GU_SCISSOR_TEST);
    sceGuDisable(GU_DEPTH_TEST);

    sceGuFinish();
    sceGuSync(0,0);
    sceDisplayWaitVblankStart();
    sceGuDisplay(GU_TRUE);

    SceCtrlData p;
    unsigned int old=0;

    while(1){
        sceCtrlReadBufferPositive(&p,1);

        if(pressed(p.Buttons,old,PSP_CTRL_START))
            paused=!paused;

        if(!paused){
            if(pressed(p.Buttons,old,PSP_CTRL_CIRCLE))
                cast();

            if(pressed(p.Buttons,old,PSP_CTRL_TRIANGLE))
                spend();

            if(pressed(p.Buttons,old,PSP_CTRL_LTRIGGER))
                selected=(selected+4)%5;

            if(pressed(p.Buttons,old,PSP_CTRL_RTRIGGER))
                selected=(selected+1)%5;

            if(pressed(p.Buttons,old,PSP_CTRL_SELECT))
                respawn();

            sword((p.Buttons&PSP_CTRL_SQUARE)!=0);
            update(p,old);
        }

        draw();
        old=p.Buttons;

        sceKernelDelayThread(16667);
    }

    sceGuTerm();
    sceKernelExitGame();
    return 0;
}
