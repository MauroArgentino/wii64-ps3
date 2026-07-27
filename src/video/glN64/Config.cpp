/**
 * glN64_GX - Config.cpp (PS3 defaults)
 * Platform-specific configuration defaults for wii64-ps3
 */

#include "Config.h"
#include "glN64.h"
#include "RSP.h"
#include "Textures.h"
#include "OpenGL.h"
#include "../../main/GameHackManager.h"

extern GameHackManager *g_game_hack_mgr;

#ifdef PS3
extern char glN64_useFrameBufferTextures;
extern char glN64_use2xSaiTextures;
#endif

void Config_LoadConfig()
{
#ifdef PS3
    OGL.fullscreenWidth = 640;
    OGL.fullscreenHeight = 480;
    OGL.windowedWidth = 640;
    OGL.windowedHeight = 480;
    OGL.forceBilinear = 0;
    OGL.enable2xSaI = glN64_use2xSaiTextures;
    OGL.fog = 1;
    OGL.textureBitDepth = 2; // 32-bit only
    OGL.frameBufferTextures = glN64_useFrameBufferTextures;
    OGL.usePolygonStipple = 0;
    cache.maxBytes = 32 * 1048576;
#else
    // Non-PS3 platforms use file-based config (handled elsewhere)
#endif
}

void Config_DoConfig()
{
#ifdef PS3
    // PS3 has no GUI config dialog - settings are fixed at compile time
    Config_LoadConfig();
#else
    // Non-PS3 platforms may show GTK dialog (Config_linux.cpp)
#endif
}