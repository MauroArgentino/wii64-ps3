#include "WiiChannelStyle.h"

void drawWiiChannelBackground(menu::Graphics& gfx, float posX, float posY, float drawW, float drawH, uint32_t color) {
    gfx.setTEV(GX_PASSCLR);
    gfx.enableBlending(true);

    // Simulación de bordes redondeados con una forma de cruz (evita huecos en las esquinas)
    float r = 8.0f;
    // Ajustar r si el canal es muy pequeño (durante el flip)
    if (drawW < r * 2.5f) r = drawW * 0.3f;

    GXColor col = {(uint8_t)(color>>24), (uint8_t)(color>>16), (uint8_t)(color>>8), (uint8_t)color};
    gfx.setColor(col);
    
    float x = posX - drawW / 2.0f;
    float y = posY - drawH / 2.0f;

    // Bloque central vertical (Cuerpo)
    gfx.fillRect((int)(x + r), (int)y, (int)(drawW - r*2), (int)drawH);
    // Bloque lateral izquierdo
    gfx.fillRect((int)x, (int)(y + r), (int)r, (int)(drawH - r*2));
    // Bloque lateral derecho
    gfx.fillRect((int)(x + drawW - r), (int)(y + r), (int)r, (int)(drawH - r*2));
}

void drawWiiSelectionFrame(menu::Graphics& gfx, float posX, float posY, float drawW, float drawH) {
    gfx.setTEV(GX_PASSCLR);
    gfx.enableBlending(true);

    // 1. Borde blanco externo (Marco sólido de 4 barras)
    int b = 4;
    float x = posX - drawW / 2.0f;
    float y = posY - drawH / 2.0f;

    gfx.setColor((GXColor){255, 255, 255, 255});
    gfx.fillRect((int)(x - b), (int)(y - b), (int)(drawW + b*2), b);     // Barra Superior
    gfx.fillRect((int)(x - b), (int)(y + drawH), (int)(drawW + b*2), b); // Barra Inferior
    gfx.fillRect((int)(x - b), (int)y, b, (int)drawH);                   // Barra Izquierda
    gfx.fillRect((int)(x + drawW), (int)y, b, (int)drawH);               // Barra Derecha

    // 2. Brillo celeste interno (Marco más delgado)
    int g = 2;
    gfx.setColor((GXColor){0, 170, 255, 255});
    gfx.fillRect((int)(x - g), (int)(y - g), (int)(drawW + g*2), g);
    gfx.fillRect((int)(x - g), (int)(y + drawH), (int)(drawW + g*2), g);
    gfx.fillRect((int)(x - g), (int)y, g, (int)drawH);
    gfx.fillRect((int)(x + drawW), (int)y, g, (int)drawH);
}