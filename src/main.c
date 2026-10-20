
/* ===== RANDOM: PERSONAGEM + ANIMACOES ===== */

#define P_BLACK 0xFF080808
#define P_DARK  0xFF242424
#define P_GRAY  0xFF626262
#define P_LIGHT 0xFFAAAAAA
#define P_WHITE 0xFFF5F5F0

/* CABELO ESPETADO */
static void hair(float x, float y, float s)
{
    triangle(x+10*s,y+43*s, x+9*s,y+15*s,
             x+22*s,y+4*s,P_BLACK);
    triangle(x+15*s,y+39*s, x+30*s,y+1*s,
             x+35*s,y+40*s,P_BLACK);
    triangle(x+27*s,y+38*s, x+43*s,y+12*s,
             x+49*s,y+35*s,P_DARK);
    triangle(x+12*s,y+35*s, x+26*s,y+24*s,
             x+42*s,y+41*s,P_GRAY);
    triangle(x+9*s,y+39*s, x+17*s,y+53*s,
             x+8*s,y+51*s,P_BLACK);
    rect(x+13*s,y+32*s,34*s,11*s,P_BLACK);
}

/* ROSTO SEM OLHOS E BOCA */
static void face(float x, float y, float s)
{
    triangle(x+15*s,y+37*s, x+40*s,y+37*s,
             x+38*s,y+60*s,P_WHITE);
    triangle(x+15*s,y+37*s, x+38*s,y+60*s,
             x+26*s,y+70*s,P_WHITE);
    triangle(x+15*s,y+37*s, x+26*s,y+70*s,
             x+12*s,y+58*s,P_WHITE);
    triangle(x+36*s,y+41*s, x+40*s,y+39*s,
             x+37*s,y+57*s,P_LIGHT);
}

/* GOLA */
static void collar(float x, float y, float s)
{
    rect(x+22*s,y+65*s,12*s,12*s,P_DARK);
    triangle(x+11*s,y+72*s, x+43*s,y+72*s,
             x+37*s,y+91*s,P_BLACK);
    triangle(x+11*s,y+72*s, x+37*s,y+91*s,
             x+7*s,y+84*s,P_GRAY);
    triangle(x+15*s,y+74*s, x+36*s,y+75*s,
             x+25*s,y+83*s,P_WHITE);
}

/* TRONCO */
static void torso(float x, float y, float s)
{
    triangle(x+13*s,y+82*s, x+25*s,y+77*s,
             x+13*s,y+111*s,P_DARK);
    triangle(x+25*s,y+77*s, x+43*s,y+83*s,
             x+39*s,y+113*s,P_BLACK);
    triangle(x+13*s,y+82*s, x+39*s,y+83*s,
             x+37*s,y+120*s,P_BLACK);
    triangle(x+13*s,y+82*s, x+37*s,y+120*s,
             x+13*s,y+117*s,P_DARK);

    /* Faixa diagonal */
    triangle(x+13*s,y+86*s, x+22*s,y+91*s,
             x+39*s,y+82*s,P_GRAY);
    triangle(x+13*s,y+86*s, x+17*s,y+100*s,
             x+22*s,y+91*s,P_LIGHT);

    rect(x+13*s,y+116*s,27*s,6*s,P_DARK);
    rect(x+17*s,y+119*s,19*s,3*s,P_GRAY);
}

/* BRACOS ANIMADOS */
static void arms(float x, float y, float s, float swing)
{
    /* Manga esquerda */
    triangle(x+13*s,y+84*s, x+23*s,y+87*s,
             x+20*s,y+108*s,P_BLACK);
    triangle(x+13*s,y+84*s, x+20*s,y+108*s,
             x+11*s,y+105*s,P_GRAY);

    /* Antebraco esquerdo */
    line(x+15*s,y+105*s,
         x+9*s,y+128*s+swing,6*s,P_DARK);
    line(x+9*s,y+128*s+swing,
         x+7*s,y+144*s+swing,4*s,P_WHITE);
    rect(x+3*s,y+140*s+swing,9*s,9*s,P_BLACK);
    rect(x+5*s,y+144*s+swing,5*s,5*s,P_WHITE);

    /* Manga direita */
    triangle(x+36*s,y+83*s, x+46*s,y+89*s,
             x+47*s,y+108*s,P_BLACK);
    triangle(x+36*s,y+83*s, x+47*s,y+108*s,
             x+37*s,y+103*s,P_DARK);

    /* Braco comprido */
    line(x+43*s,y+105*s,
         x+49*s,y+126*s-swing,7*s,P_BLACK);
    line(x+49*s,y+126*s-swing,
         x+56*s,y+146*s-swing,5*s,P_DARK);
    line(x+56*s,y+146*s-swing,
         x+60*s,y+161*s-swing,3*s,P_BLACK);

    triangle(x+55*s,y+155*s-swing,
             x+63*s,y+158*s-swing,
             x+59*s,y+169*s-swing,P_BLACK);
    triangle(x+55*s,y+155*s-swing,
             x+59*s,y+169*s-swing,
             x+54*s,y+163*s-swing,P_WHITE);
}

/* CALCA LARGA E TENIS */
static void legs(float x, float y, float s, float step)
{
    /* Perna esquerda */
    triangle(x+13*s,y+120*s, x+28*s,y+120*s,
             x+25*s,y+147*s,P_BLACK);
    triangle(x+13*s,y+120*s, x+25*s,y+147*s,
             x+8*s,y+162*s,P_DARK);
    triangle(x+8*s,y+162*s, x+25*s,y+147*s,
             x+23*s,y+174*s,P_GRAY);
    triangle(x+8*s,y+162*s, x+23*s,y+174*s,
             x+10*s,y+174*s,P_WHITE);

    /* Perna direita */
    triangle(x+27*s,y+120*s, x+40*s,y+120*s,
             x+48*s,y+158*s,P_DARK);
    triangle(x+27*s,y+120*s, x+48*s,y+158*s,
             x+37*s,y+172*s,P_GRAY);

    /* Faixa clara da calca */
    triangle(x+29*s,y+125*s, x+34*s,y+128*s,
             x+44*s,y+158*s,P_WHITE);
    triangle(x+34*s,y+128*s, x+44*s,y+158*s,
             x+40*s,y+162*s,P_LIGHT);

    /* Movimento das pernas */
    line(x+15*s,y+171*s,
         x+14*s+step,y+178*s,4*s,P_BLACK);
    line(x+42*s,y+171*s,
         x+43*s-step,y+178*s,4*s,P_BLACK);

    /* Tenis */
    triangle(x+8*s+step,y+176*s,
             x+20*s+step,y+176*s,
             x+22*s+step,y+180*s,P_WHITE);
    rect(x+5*s+step,y+178*s,17*s,2*s,P_BLACK);

    triangle(x+39*s-step,y+176*s,
             x+48*s-step,y+176*s,
             x+51*s-step,y+180*s,P_WHITE);
    rect(x+38*s-step,y+178*s,15*s,2*s,P_BLACK);
}

/* PERSONAGEM COMPLETO */
static void drawPlayer(Player *p)
{
    float x = p->x;
    float y = p->y;
    float s = p->scale;
    float swing = 0.0f;
    float step = 0.0f;

    /* Corrida */
    if (p->anim == RUN) {
        swing = sinf(p->animTime * 12.0f) * 5.0f;
        step = sinf(p->animTime * 12.0f) * 4.0f;
    }

    /* Salto: pernas recolhidas visualmente */
    if (p->anim == JUMP) {
        step = -2.0f;
        swing = -2.0f;
    }

    /* Dash */
    if (p->anim == DASH) {
        x -= p->direction * 3.0f;
    }

    legs(x,y,s,step);
    collar(x,y,s);
    torso(x,y,s);
    arms(x,y,s,swing);
    face(x,y,s);
    hair(x,y,s);

    /* Ataque frontal: meia-lua */
    if (p->anim == HIT && p->attackTimer > 0) {
        float sign = (float)p->direction;
        float ax = x + (sign > 0 ? 44*s : -15*s);

        line(ax,y+94*s,
             ax+14*s*sign,y+78*s,4*s,P_WHITE);
        line(ax+14*s*sign,y+78*s,
             ax+29*s*sign,y+89*s,3*s,P_LIGHT);
        line(ax+4*s*sign,y+101*s,
             ax+24*s*sign,y+98*s,2*s,P_WHITE);
    }
}
