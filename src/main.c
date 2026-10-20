
/* ===== RANDOM: PERSONAGEM CARTOON ===== */

static void hair(float x, float y, float s)
{
    /* Cabelo grande e espetado */
    triangle(x+7*s,y+36*s, x+5*s,y+12*s,
             x+19*s,y+20*s, BLACK);
    triangle(x+12*s,y+30*s, x+25*s,y+2*s,
             x+31*s,y+31*s, BLACK);
    triangle(x+25*s,y+30*s, x+43*s,y+9*s,
             x+45*s,y+35*s, DARK);
    triangle(x+8*s,y+31*s, x+25*s,y+20*s,
             x+43*s,y+34*s, GRAY);

    rect(x+9*s,y+27*s,37*s,12*s,BLACK);
}

static void face(float x, float y, float s)
{
    /* Cabeca maior, estilo cartoon */
    rect(x+12*s,y+32*s,29*s,28*s,WHITE);
    rect(x+16*s,y+27*s,21*s,7*s,WHITE);
    triangle(x+12*s,y+40*s, x+18*s,y+58*s,
             x+12*s,y+55*s,LIGHT);
    triangle(x+41*s,y+38*s, x+41*s,y+55*s,
             x+35*s,y+60*s,LIGHT);
}

static void collar(float x, float y, float s)
{
    rect(x+22*s,y+58*s,11*s,9*s,DARK);
    triangle(x+12*s,y+64*s, x+42*s,y+64*s,
             x+38*s,y+78*s,BLACK);
    triangle(x+12*s,y+64*s, x+38*s,y+78*s,
             x+16*s,y+78*s,GRAY);
    triangle(x+17*s,y+65*s, x+35*s,y+65*s,
             x+26*s,y+73*s,WHITE);
}

static void torso(float x, float y, float s)
{
    /* Tronco compacto */
    triangle(x+14*s,y+72*s, x+39*s,y+72*s,
             x+43*s,y+105*s,BLACK);
    triangle(x+14*s,y+72*s, x+43*s,y+105*s,
             x+11*s,y+105*s,DARK);

    /* Faixa clara da roupa */
    triangle(x+14*s,y+77*s, x+23*s,y+82*s,
             x+39*s,y+73*s,GRAY);
    triangle(x+14*s,y+77*s, x+18*s,y+92*s,
             x+23*s,y+82*s,LIGHT);

    rect(x+12*s,y+102*s,30*s,6*s,BLACK);
}

static void arms(float x, float y, float s, float swing)
{
    /* Bracos mais curtos e expressivos */
    line(x+15*s,y+76*s,
         x+7*s,y+94*s+swing,7*s,BLACK);
    line(x+7*s,y+94*s+swing,
         x+8*s,y+111*s+swing,5*s,DARK);

    rect(x+3*s,y+108*s+swing,10*s,9*s,BLACK);
    rect(x+5*s,y+111*s+swing,6*s,4*s,WHITE);

    line(x+39*s,y+76*s,
         x+47*s,y+94*s-swing,7*s,BLACK);
    line(x+47*s,y+94*s-swing,
         x+52*s,y+109*s-swing,5*s,DARK);

    rect(x+48*s,y+106*s-swing,10*s,9*s,BLACK);
    rect(x+51*s,y+109*s-swing,5*s,4*s,WHITE);
}

static void legs(float x, float y, float s, float step)
{
    /* Calca larga e arredondada visualmente */
    triangle(x+13*s,y+105*s, x+28*s,y+105*s,
             x+25*s,y+130*s,DARK);
    triangle(x+28*s,y+105*s, x+41*s,y+105*s,
             x+45*s,y+130*s,BLACK);

    /* Faixa branca */
    triangle(x+29*s,y+109*s, x+34*s,y+110*s,
             x+41*s,y+128*s,WHITE);

    /* Pernas */
    line(x+19*s,y+128*s,
         x+17*s+step,y+146*s,5*s,BLACK);
    line(x+39*s,y+128*s,
         x+41*s-step,y+146*s,5*s,BLACK);

    /* Tenis maiores */
    triangle(x+8*s+step,y+143*s,
             x+22*s+step,y+143*s,
             x+25*s+step,y+150*s,WHITE);
    rect(x+7*s+step,y+148*s,19*s,4*s,BLACK);

    triangle(x+35*s-step,y+143*s,
             x+47*s-step,y+143*s,
             x+52*s-step,y+150*s,WHITE);
    rect(x+35*s-step,y+148*s,18*s,4*s,BLACK);
}

static void drawPlayer(Player *p)
{
    float x = p->x;
    float y = p->y;
    float s = p->scale;
    float swing = 0.0f;
    float step = 0.0f;

    if (p->anim == RUN) {
        swing = sinf(p->animTime * 12.0f) * 5.0f;
        step = sinf(p->animTime * 12.0f) * 4.0f;
    }

    if (p->anim == JUMP) {
        swing = -3.0f;
        step = -2.0f;
    }

    legs(x,y,s,step);
    torso(x,y,s);
    collar(x,y,s);
    arms(x,y,s,swing);
    face(x,y,s);
    hair(x,y,s);

    /* Golpe cartoon: tres riscos */
    if (p->anim == HIT && p->attackTimer > 0) {
        float sign = (float)p->direction;
        float ax = x + (sign > 0 ? 47*s : -10*s);

        line(ax,y+79*s,
             ax+13*s*sign,y+68*s,4*s,WHITE);
        line(ax+5*s*sign,y+88*s,
             ax+22*s*sign,y+82*s,3*s,LIGHT);
        line(ax+9*s*sign,y+96*s,
             ax+23*s*sign,y+96*s,2*s,WHITE);
    }
}
