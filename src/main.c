/* =====================================================
   GEOMETRIA SUAVE PARA A ESSENCIA ESCURA
   ===================================================== */

static void drawCircle(
    float cx, float cy, float radius,
    unsigned int color
) {
    int i;
    const int segments = 16;
    const float PI = 3.14159265f;

    for (i = 0; i < segments; i++) {
        float a0 = 2.0f * PI * i / segments;
        float a1 = 2.0f * PI * (i + 1) / segments;

        drawTriangle(
            cx, cy,
            cx + cosf(a0) * radius,
            cy + sinf(a0) * radius,
            cx + cosf(a1) * radius,
            cy + sinf(a1) * radius,
            color
        );
    }
}

static void drawLine(
    float x1, float y1,
    float x2, float y2,
    float thickness,
    unsigned int color
) {
    float dx = x2 - x1;
    float dy = y2 - y1;
    float len = sqrtf(dx * dx + dy * dy);

    if (len < 0.001f)
        return;

    float nx = -dy / len * thickness * 0.5f;
    float ny =  dx / len * thickness * 0.5f;

    drawTriangle(
        x1 + nx, y1 + ny,
        x1 - nx, y1 - ny,
        x2 + nx, y2 + ny,
        color
    );

    drawTriangle(
        x1 - nx, y1 - ny,
        x2 - nx, y2 - ny,
        x2 + nx, y2 + ny,
        color
    );
}

/* Poligono preenchido usando triangulos */
static void drawPolygon(
    const float *points,
    int count,
    unsigned int color
) {
    int i;

    if (count < 3)
        return;

    for (i = 1; i < count - 1; i++) {
        drawTriangle(
            points[0], points[1],
            points[i * 2], points[i * 2 + 1],
            points[(i + 1) * 2],
            points[(i + 1) * 2 + 1],
            color
        );
    }
}
