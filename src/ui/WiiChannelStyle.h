#ifndef WIICHANNELSTYLE_H
#define WIICHANNELSTYLE_H

#include "libgui/GuiTypes.h"

#ifdef __GX__
#include "libgui/GraphicsGX.h"
#else
#include "libgui/GraphicsRSX.h"
#endif

void drawWiiChannelBackground(menu::Graphics& gfx, float posX, float posY, float drawW, float drawH, uint32_t color);
void drawWiiSelectionFrame(menu::Graphics& gfx, float posX, float posY, float drawW, float drawH);

#endif