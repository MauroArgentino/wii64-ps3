#include "MenuElementStyle.h"
#include <math.h>

void drawChannelBackground(menu::Graphics& gfx, float posX, float posY, float drawW, float drawH, uint32_t color) {
    gfx.setTEV(GX_PASSCLR);
    gfx.enableBlending(true);

    int r = 6;
    if (drawW < r * 2.5f) r = (int)(drawW * 0.3f);
    if (r < 1) r = 1;

    GXColor col = {(uint8_t)(color>>24), (uint8_t)(color>>16), (uint8_t)(color>>8), (uint8_t)color};
    gfx.setColor(col);

    int x = (int)(posX - drawW / 2.0f);
    int y = (int)(posY - drawH / 2.0f);
    gfx.fillRoundedRect(x, y, (int)drawW, (int)drawH, r, 8);
}

void drawChannelStatic(menu::Graphics& gfx, float posX, float posY, float drawW, float drawH, float time) {
    gfx.enableStaticShader(time);
    gfx.enableBlending(true);

    int r = 6;
    if (drawW < r * 2.5f) r = (int)(drawW * 0.3f);
    if (r < 1) r = 1;

    gfx.setColor((GXColor){255, 255, 255, 255});

    int x = (int)(posX - drawW / 2.0f);
    int y = (int)(posY - drawH / 2.0f);
    gfx.fillRoundedRect(x, y, (int)drawW, (int)drawH, r, 8);

    gfx.disableStaticShader();
}

void drawSelectionFrame(menu::Graphics& gfx, float posX, float posY, float drawW, float drawH, uint32_t cardColor) {
    gfx.setTEV(GX_PASSCLR);
    gfx.enableBlending(true);

    int r = 6;
    if (drawW < r * 2.5f) r = (int)(drawW * 0.3f);
    if (r < 1) r = 1;

    int x = (int)(posX - drawW / 2.0f);
    int y = (int)(posY - drawH / 2.0f);
    int w = (int)drawW;
    int h = (int)drawH;

    // Layer 1: White outer border (largest rounded rect)
    gfx.setColor((GXColor){255, 255, 255, 255});
    gfx.fillRoundedRect(x - 4, y - 4, w + 8, h + 8, r + 4, 8);

    // Layer 2: Cyan glow (medium rounded rect)
    gfx.setColor((GXColor){0, 170, 255, 255});
    gfx.fillRoundedRect(x - 2, y - 2, w + 4, h + 4, r + 2, 8);

    // Layer 3: Card color covers center (same size as card, hides the middle)
    GXColor card = {(uint8_t)(cardColor>>24), (uint8_t)(cardColor>>16),
                    (uint8_t)(cardColor>>8), (uint8_t)cardColor};
    gfx.setColor(card);
    gfx.fillRoundedRect(x, y, w, h, r, 8);
}
