#ifndef MENUELEMENTSTYLE_H
#define MENUELEMENTSTYLE_H

#include "libgui/GuiTypes.h" // Asumiendo que GuiTypes es genérico

#ifdef __GX__
#include "libgui/GraphicsGX.h"
#else
#include "libgui/GraphicsRSX.h"
#endif

void drawChannelBackground(menu::Graphics& gfx, float posX, float posY, float drawW, float drawH, uint32_t color); // Renombrado
void drawChannelStatic(menu::Graphics& gfx, float posX, float posY, float drawW, float drawH, float time);
void drawSelectionFrame(menu::Graphics& gfx, float posX, float posY, float drawW, float drawH, uint32_t cardColor);

#endif // MENUELEMENTSTYLE_H