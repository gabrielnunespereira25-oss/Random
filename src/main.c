/* =========================================================
   RANDOM - CENARIO MONOCROMATICO COMPLETO
   PSP: 480x272
   Zona 1: Caverna
   Zona 2: Floresta
   Sem PNG, GIF ou texturas externas
   ========================================================= */

#define C_BLACK  0xFF080808
#define C_DARK   0xFF252525
#define C_STONE  0xFF555555
#define C_GRAY   0xFF858585
#define C_LIGHT  0xFFBFBFBF
#define C_WHITE  0xFFFFFFFF

/* =========================================================
   TRIANGULOS
   ========================================================= */

static void triangle(
    float x1, float y1,
    float x2, float y2,
    float x3, float y3,
    Color color)
{
    Vtx *v = (Vtx*)sceGuGetMemory(3 * sizeof(Vtx));

    v[0] = (Vtx){color, x1, y1, 0};
    v[1] = (Vtx){color, x2, y2, 0};
    v[2] = (Vtx){color, x3, y3, 0};

    sceGuDrawArray(
        GU_TRIANGLES,
        GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_2D,
        3, 0, v
    );
}

static void wtriangle(
    float x1, float y1,
    float x2, float y2,
    float x3, float y3,
    Color color)
{
    triangle(
        x1 - cam, y1,
        x2 - cam, y2,
        x3 - cam, y3,
        color
    );
}

/* =========================================================
   PLACAS DE SINALIZACAO
   ========================================================= */

static void drawSign(float x, float y, int direction)
{
    wr(x, y, 24, 17, C_DARK);
    wr(x + 2, y + 2, 20, 13, C_LIGHT);

    if (direction > 0) {
        wr(x + 5, y + 7, 11, 3, C_BLACK);

        triangle(
            x + 15 - cam, y + 3,
            x + 22 - cam, y + 8,
            x + 15 - cam, y + 13,
            C_BLACK
        );
    } else {
        wr(x + 8, y + 7, 11, 3, C_BLACK);

        triangle(
            x + 9 - cam, y + 3,
            x + 2 - cam, y + 8,
            x + 9 - cam, y + 13,
            C_BLACK
        );
    }

    wr(x + 10, y + 17, 4, 14, C_STONE);
}

/* =========================================================
   INIMIGOS
   type 0 = rastejante
   type 1 = bipedal
   type 2 = blindado
   ========================================================= */

static void drawMob(float x, float y, int type)
{
    if (type == 0) {
        /* Rastejante */
        wr(x - 3, y - 2, 29, 3, C_BLACK);
        wr(x, y - 17, 23, 14, C_STONE);
        wr(x + 3, y - 14, 15, 4, C_LIGHT);

        wr(x + 13, y - 21, 10, 9, C_GRAY);
        wr(x + 19, y - 18, 3, 3, C_WHITE);
        wr(x + 20, y - 18, 2, 2, C_BLACK);

        wr(x + 2, y - 4, 4, 4, C_LIGHT);
        wr(x + 9, y - 4, 4, 4, C_LIGHT);
        wr(x + 17, y - 4, 4, 4, C_LIGHT);

        wtriangle(
            x + 3, y - 17,
            x + 8, y - 24,
            x + 12, y - 17,
            C_WHITE
        );
    }
    else if (type == 1) {
        /* Criatura bípede */
        wr(x + 3, y - 9, 5, 9, C_DARK);
        wr(x + 12, y - 9, 5, 9, C_DARK);

        wr(x + 2, y - 22, 16, 14, C_GRAY);
        wr(x + 5, y - 20, 10, 9, C_LIGHT);

        wr(x - 2, y - 20, 5, 12, C_STONE);
        wr(x + 16, y - 20, 5, 12, C_STONE);

        wr(x + 2, y - 29, 15, 12, C_STONE);

        wtriangle(
            x + 3, y - 26,
            x - 2, y - 34,
            x + 7, y - 28,
            C_LIGHT
        );

        wtriangle(
            x + 14, y - 26,
            x + 21, y - 34,
            x + 12, y - 28,
            C_LIGHT
        );

        wr(x + 5, y - 24, 4, 4, C_WHITE);
        wr(x + 12, y - 24, 4, 4, C_WHITE);
        wr(x + 6, y - 23, 2, 2, C_BLACK);
        wr(x + 13, y - 23, 2, 2, C_BLACK);
        wr(x + 7, y - 18, 7, 2, C_BLACK);
    }
    else {
        /* Bicho blindado */
        wr(x + 2, y - 17, 23, 13, C_DARK);
        wr(x + 5, y - 21, 17, 8, C_LIGHT);

        wr(x + 5, y - 12, 6, 5, C_GRAY);
        wr(x + 15, y - 12, 6, 5, C_GRAY);

        wtriangle(
            x + 5, y - 17,
            x - 1, y - 26,
            x + 10, y - 19,
            C_WHITE
        );

        wtriangle(
            x + 21, y - 17,
            x + 28, y - 26,
            x + 17, y - 19,
            C_WHITE
        );

        wr(x + 7, y - 15, 4, 3, C_WHITE);
        wr(x + 17, y - 15, 4, 3, C_WHITE);

        wr(x + 4, y - 5, 6, 5, C_STONE);
        wr(x + 17, y - 5, 6, 5, C_STONE);
    }
}

/* =========================================================
   MINIBOSS
   ========================================================= */

static void drawMiniBoss(float x, float y)
{
    wr(x - 5, y - 2, 46, 4, C_BLACK);

    /* Pernas */
    wr(x + 5, y - 17, 10, 17, C_DARK);
    wr(x + 25, y - 17, 10, 17, C_DARK);

    /* Corpo */
    wr(x + 3, y - 43, 34, 27, C_STONE);
    wr(x + 7, y - 40, 26, 19, C_GRAY);
    wr(x + 12, y - 36, 16, 10, C_DARK);

    /* Bracos */
    wr(x - 5, y - 37, 10, 22, C_DARK);
    wr(x + 35, y - 37, 10, 22, C_DARK);

    /* Cabeca */
    wr(x + 7, y - 58, 26, 19, C_LIGHT);

    /* Chifres */
    wtriangle(
        x + 9, y - 53,
        x + 1, y - 65,
        x + 16, y - 56,
        C_WHITE
    );

    wtriangle(
        x + 31, y - 53,
        x + 40, y - 65,
        x + 25, y - 56,
        C_WHITE
    );

    /* Olhos e boca */
    wr(x + 11, y - 51, 7, 6, C_BLACK);
    wr(x + 23, y - 51, 7, 6, C_BLACK);
    wr(x + 13, y - 50, 3, 3, C_WHITE);
    wr(x + 25, y - 50, 3, 3, C_WHITE);

    wr(x + 14, y - 42, 12, 4, C_BLACK);
    wr(x + 16, y - 42, 3, 3, C_WHITE);
    wr(x + 22, y - 42, 3, 3, C_WHITE);

    /* Placas da armadura */
    wr(x + 9, y - 29, 22, 3, C_WHITE);
    wr(x + 15, y - 24, 10, 3, C_BLACK);
}

/* =========================================================
   ZONA 1: CAVERNA
   ========================================================= */

static void drawCave(void)
{
    int i;

    rect(0, 0, SW, SH, C_BLACK);

    /* Teto */
    wr(0, 0, WW, 28, C_DARK);
    wr(0, 28, WW, 5, C_STONE);

    for (i = 0; i < 32; i++) {
        float x = (float)(i * 53);

        wtriangle(
            x, 28,
            x + 17, 28,
            x + 8, 42 + (i % 4) * 5,
            (i % 2) ? C_STONE : C_DARK
        );
    }

    /* Rochas no fundo */
    for (i = 0; i < 45; i++) {
        float x = (float)(i * 37);
        float y = 65 + (i % 4) * 23;

        wr(x, y, 24, 3, C_DARK);
        wr(x + 8, y + 3, 3, 12, C_STONE);
    }

    /* Chao */
    wr(0, GROUND, WW, 22, C_STONE);
    wr(0, GROUND, WW, 4, C_LIGHT);
    wr(0, GROUND + 22, WW, 30, C_DARK);

    for (i = 0; i < 40; i++) {
        float x = (float)(i * 41 + 7);

        wr(x, GROUND + 7, 13, 3, C_GRAY);
        wr(x + 18, GROUND + 14, 7, 3, C_BLACK);
    }

    /* Plataformas */
    wr(140, 205, 105, 8, C_STONE);
    wr(140, 205, 105, 3, C_LIGHT);

    wr(290, 177, 115, 8, C_STONE);
    wr(290, 177, 115, 3, C_LIGHT);

    wr(435, 211, 95, 8, C_STONE);
    wr(435, 211, 95, 3, C_LIGHT);

    wr(540, 155, 120, 8, C_STONE);
    wr(540, 155, 120, 3, C_LIGHT);

    wr(675, 122, 105, 8, C_STONE);
    wr(675, 122, 105, 3, C_LIGHT);

    wr(790, 180, 115, 8, C_STONE);
    wr(790, 180, 115, 3, C_LIGHT);

    wr(925, 148, 110, 8, C_STONE);
    wr(925, 148, 110, 3, C_LIGHT);

    wr(1050, 205, 110, 8, C_STONE);
    wr(1050, 205, 110, 3, C_LIGHT);

    wr(1175, 166, 105, 8, C_STONE);
    wr(1175, 166, 105, 3, C_LIGHT);

    wr(1310, 198, 100, 8, C_STONE);
    wr(1310, 198, 100, 3, C_LIGHT);

    /* Rochas pontudas */
    wtriangle(230, 250, 265, 250, 246, 218, C_DARK);
    wtriangle(395, 250, 430, 250, 412, 225, C_STONE);
    wtriangle(875, 250, 913, 250, 896, 222, C_DARK);
    wtriangle(1110, 250, 1145, 250, 1125, 226, C_STONE);

    /* Cristal magico */
    wtriangle(590, 139, 602, 119, 614, 139, C_LIGHT);
    wtriangle(590, 139, 602, 158, 614, 139, C_GRAY);
    wr(600, 137, 4, 4, C_WHITE);

    /* Inimigos da caverna */
    drawMob(180, GROUND, 0);
    drawMob(350, GROUND, 1);
    drawMob(570, GROUND, 2);
    drawMob(850, GROUND, 0);

    /* Miniboss */
    drawMiniBoss(1350, GROUND);

    /* Saida inferior para a floresta */
    wr(1470, 188, 70, 62, C_BLACK);
    wr(1470, 188, 5, 62, C_STONE);
    wr(1535, 188, 5, 62, C_STONE);
    wr(1470, 188, 70, 5, C_LIGHT);

    drawSign(1430, 210, 1);

    /* Saida elevada */
    wr(680, 70, 65, 52, C_BLACK);
    wr(680, 70, 5, 52, C_STONE);
    wr(740, 70, 5, 52, C_STONE);
    wr(680, 70, 65, 4, C_LIGHT);

    drawSign(645, 83, 1);
}

/* =========================================================
   ARVORES DA FLORESTA
   ========================================================= */

static void drawTree(float x, float baseY, int variant)
{
    float h = 50 + (variant % 3) * 9;
    float w = 28 + (variant % 2) * 10;
    float top = baseY - h;

    /* Tronco */
    wr(x + w * 0.38f, top + h * 0.42f,
       w * 0.24f, h * 0.58f, C_DARK);

    /* Raizes */
    wtriangle(
        x + w * 0.38f, baseY - 8,
        x + w * 0.08f, baseY,
        x + w * 0.43f, baseY,
        C_STONE
    );

    wtriangle(
        x + w * 0.62f, baseY - 8,
        x + w * 0.92f, baseY,
        x + w * 0.57f, baseY,
        C_STONE
    );

    /* Galhos */
    wr(x + w * 0.13f, top + h * 0.53f,
       w * 0.34f, 3, C_STONE);

    wr(x + w * 0.53f, top + h * 0.35f,
       w * 0.34f, 3, C_STONE);

    /* Copa */
    wtriangle(
        x + w * 0.50f, top,
        x, top + h * 0.52f,
        x + w, top + h * 0.52f,
        C_STONE
    );

    wtriangle(
        x + w * 0.50f, top + h * 0.16f,
        x - 5, top + h * 0.70f,
        x + w * 0.72f, top + h * 0.66f,
        C_GRAY
    );

    wtriangle(
        x + w * 0.48f, top + h * 0.22f,
        x + w * 0.25f, top + h * 0.72f,
        x + w + 5, top + h * 0.62f,
        C_DARK
    );

    /* Texturas */
    wr(x + w * 0.26f, top + h * 0.40f,
       w * 0.19f, 3, C_LIGHT);

    wr(x + w * 0.55f, top + h * 0.54f,
       w * 0.18f, 3, C_LIGHT);
}

/* =========================================================
   ZONA 2: FLORESTA
   ========================================================= */

static void drawForest(void)
{
    int i;

    /* Ceu noturno */
    rect(0, 0, SW, SH, C_BLACK);

    /* Lua */
    rect(360, 27, 24, 24, C_GRAY);
    rect(367, 22, 19, 24, C_BLACK);
    rect(355, 31, 5, 5, C_WHITE);

    /* Montanhas */
    wtriangle(0, 172, 140, 48, 280, 172, C_DARK);
    wtriangle(180, 172, 370, 63, 560, 172, C_STONE);
    wtriangle(450, 172, 610, 82, 790, 172, C_DARK);
    wtriangle(750, 172, 940, 53, 1110, 172, C_STONE);
    wtriangle(1050, 172, 1250, 73, 1440, 172, C_DARK);
    wtriangle(1360, 172, 1500, 62, 1600, 172, C_STONE);

    /* Arvores distantes */
    for (i = 0; i < 24; i++) {
        float x = (float)(i * 69 + 12);
        float y = 203 + (i % 3) * 7;

        wr(x + 12, y - 35, 5, 35, C_DARK);

        wtriangle(
            x + 14, y - 58,
            x - 5, y - 28,
            x + 33, y - 28,
            C_DARK
        );

        wtriangle(
            x + 14, y - 48,
            x - 1, y - 22,
            x + 29, y - 22,
            C_STONE
        );
    }

    /* Chao */
    wr(0, GROUND, WW, 22, C_DARK);
    wr(0, GROUND, WW, 4, C_LIGHT);
    wr(0, GROUND + 22, WW, 30, C_BLACK);

    /* Grama e pedras */
    for (i = 0; i < 55; i++) {
        float x = (float)(i * 31 + 4);

        wtriangle(
            x, GROUND,
            x + 5, GROUND - 7,
            x + 10, GROUND,
            C_GRAY
        );

        wr(x + 13, GROUND + 9, 7, 3, C_STONE);
    }

    /* Arvores principais */
    drawTree(100, 250, 0);
    drawTree(220, 250, 1);
    drawTree(365, 250, 2);
    drawTree(510, 250, 0);
    drawTree(670, 250, 1);
    drawTree(835, 250, 2);
    drawTree(1000, 250, 0);
    drawTree(1150, 250, 1);
    drawTree(1320, 250, 2);
    drawTree(1480, 250, 0);

    /* Plataformas */
    wr(180, 210, 100, 8, C_STONE);
    wr(180, 210, 100, 3, C_LIGHT);

    wr(335, 180, 100, 8, C_STONE);
    wr(335, 180, 100, 3, C_LIGHT);

    wr(500, 205, 95, 8, C_STONE);
    wr(500, 205, 95, 3, C_LIGHT);

    wr(800, 174, 110, 8, C_STONE);
    wr(800, 174, 110, 3, C_LIGHT);

    wr(1000, 205, 100, 8, C_STONE);
    wr(1000, 205, 100, 3, C_LIGHT);

    wr(1210, 115, 105, 8, C_STONE);
    wr(1210, 115, 105, 3, C_LIGHT);

    /* Passagem inferior de retorno */
    wr(65, 188, 65, 62, C_BLACK);
    wr(65, 188, 5, 62, C_STONE);
    wr(125, 188, 5, 62, C_STONE);
    wr(65, 188, 65, 5, C_LIGHT);

    drawSign(135, 210, -1);

    /* Passagem elevada de retorno */
    wr(1240, 65, 65, 50, C_BLACK);
    wr(1240, 65, 5, 50, C_STONE);
    wr(1300, 65, 5, 50, C_STONE);
    wr(1240, 65, 65, 4, C_LIGHT);

    drawSign(1205, 78, -1);
}
