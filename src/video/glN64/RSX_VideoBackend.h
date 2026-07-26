/**
 * RSX Video Backend Bridge for glN64
 * Translates N64 RDP commands to PS3 RSX calls.
 * Supports configurable internal resolution with 1080p UI overlay.
 */

#ifndef RSX_VIDEO_BACKEND_H
#define RSX_VIDEO_BACKEND_H

#include <stdint.h>
#include <ppu-types.h>

/* Forward declaration */
struct GameHackManager;

#ifdef __cplusplus
extern "C" {
#endif
void RSX_VideoInit(void);
#ifdef __cplusplus
}
#endif

void RSX_Set3DViewport(void);
void RSX_SetUIViewport(void);
void RSX_SetRenderTarget3D(void);
void RSX_SetRenderTargetUI(void);
void RSX_EnableHUDAlphaTest(void);
void RSX_DisableAlphaTest(void);
void RSX_EnableBlending(void);
void RSX_DisableBlending(void);
void RSX_ApplyGameHacks(void);
void RSX_ClearFramebuffer(uint32_t color);
void RSX_SetTextureUnpackAlignment(uint32_t alignment);
void RSX_BindTexture(uint32_t offset, uint32_t width, uint32_t height, uint32_t stride, uint32_t format);
void RSX_UpdateScreen(void);
void RSX_GetVideoConfig(uint32_t *internal_w, uint32_t *internal_h,
                         uint32_t *display_w, uint32_t *display_h);
void RSX_DrawTriangle(float* v0, float* v1, float* v2, bool texEnabled);

#endif /* RSX_VIDEO_BACKEND_H */
