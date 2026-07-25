/**
 * RSX Video Backend Bridge for glN64
 * Translates N64 RDP commands to PS3 RSX calls.
 * Supports configurable internal resolution with 1080p UI overlay.
 */

#include <rsx/rsx.h>
#include "../../ui/libgui/GraphicsRSX.h"
#include "../../main/rsxutil.h"
#include "../../main/GameHackManager.h"

extern s32 globalTextureUnit_id;

extern s32 vertexPosition_id;
extern s32 vertexColor0_id;
extern s32 vertexTexcoord_id;

void RSX_VideoInit() {
    if (g_video_config.initialized) return;

    g_video_config.display_w = display_width;
    g_video_config.display_h = display_height;

    uint32_t fb_pitch = (display_width * 4 + 63) & ~63;
    uint32_t fb_size = fb_pitch * display_height;

    g_video_config.fb_addr[0] = rsxMemalign(64, fb_size * 2);
    g_video_config.fb_addr[1] = (uint8_t*)g_video_config.fb_addr[0] + fb_size;

    for (int i = 0; i < 2; i++) {
        rsxAddressToOffset(g_video_config.fb_addr[i], &g_video_config.fb_offset[i]);
    }

    g_video_config.fb_width = display_width;
    g_video_config.fb_height = display_height;
    g_video_config.fb_pitch = fb_pitch;
    g_video_config.fb_offset[0] = 0;
    g_video_config.fb_offset[1] = fb_pitch * display_height;

    g_video_config.initialized = 1;
}

void RSX_Set3DViewport() {
    if (!g_video_config.initialized) RSX_VideoInit();

    rsxSetViewport(context,
        0, 0,
        g_video_config.internal_w, g_video_config.internal_h,
        0.0f, 1.0f,
        (float[]){
            g_video_config.internal_w * 0.5f,
            -g_video_config.internal_h * 0.5f,
            0.5f, 0.0f
        },
        (float[]){
            g_video_config.internal_w * 0.5f,
            g_video_config.internal_h * 0.5f,
            0.5f, 0.0f
        }
    );

    rsxSetScissor(context, 0, 0, g_video_config.internal_w, g_video_config.internal_h);
}

void RSX_SetUIViewport() {
    if (!g_video_config.initialized) RSX_VideoInit();

    rsxSetViewport(context,
        0, 0,
        g_video_config.display_w, g_video_config.display_h,
        0.0f, 1.0f,
        (float[]){
            g_video_config.display_w * 0.5f,
            -g_video_config.display_h * 0.5f,
            0.5f, 0.0f
        },
        (float[]){
            g_video_config.display_w * 0.5f,
            g_video_config.display_h * 0.5f,
            0.5f, 0.0f
        }
    );

    rsxSetScissor(context, 0, 0, g_video_config.display_w, g_video_config.display_h);
}

void RSX_SetRenderTarget3D() {
    if (!g_video_config.initialized) RSX_VideoInit();
    setRenderTarget(curr_fb);
}

void RSX_SetRenderTargetUI() {
    if (!g_video_config.initialized) RSX_VideoInit();
    setRenderTarget(curr_fb);
}

void RSX_EnableHUDAlphaTest() {
    rsxSetAlphaTestEnable(context, GCM_TRUE);
    rsxSetAlphaTestFunc(context, 0x0204);
    rsxSetAlphaTestRef(context, 25);
}

void RSX_DisableAlphaTest() {
    rsxSetAlphaTestEnable(context, GCM_FALSE);
}

void RSX_EnableBlending() {
    rsxSetBlendEnable(context, GCM_TRUE);
    rsxSetBlendFunc(context, GCM_SRC_ALPHA, GCM_ONE_MINUS_SRC_ALPHA, GCM_SRC_ALPHA, GCM_ONE_MINUS_SRC_ALPHA);
}

void RSX_DisableBlending() {
    rsxSetBlendEnable(context, GCM_FALSE);
    rsxSetBlendFunc(context, GCM_ONE, GCM_ZERO, GCM_ONE, GCM_ZERO);
}

void RSX_ApplyGameHacks() {
    extern GameHackManager *g_game_hack_mgr;
    const GameHacks *hacks = g_game_hack_mgr ? GameHackManager_GetHacks(g_game_hack_mgr) : NULL;
    if (!hacks) return;

    if (hacks->alphaBlendMode == 1) {
        rsxSetAlphaTestEnable(context, GCM_TRUE);
        rsxSetAlphaTestFunc(context, 0x0204);
        rsxSetAlphaTestRef(context, 25);
    } else if (hacks->alphaBlendMode == 2) {
        rsxSetAlphaTestEnable(context, GCM_TRUE);
        rsxSetAlphaTestFunc(context, 0x0206);
        rsxSetAlphaTestRef(context, 1);
    }

    if (hacks->forceAlphaTest) {
        rsxSetAlphaTestEnable(context, GCM_TRUE);
        if (!hacks->alphaBlendMode) {
            rsxSetAlphaTestFunc(context, 0x0206);
            rsxSetAlphaTestRef(context, 1);
        }
    }

    if (hacks->forceOpaqueAlphaCvg) {
    }

    if (hacks->internalWidth && hacks->internalHeight) {
        RSX_SetInternalResolution(hacks->internalWidth, hacks->internalHeight);
    }
}

void RSX_ClearFramebuffer(uint32_t color) {
    rsxSetClearColor(context, color);
    rsxSetClearDepthValue(context, 0xFFFFFF00);
    rsxClearSurface(context, GCM_CLEAR_R | GCM_CLEAR_G | GCM_CLEAR_B | GCM_CLEAR_A | GCM_CLEAR_Z | GCM_CLEAR_S);
}

void RSX_SetTextureUnpackAlignment(uint32_t alignment) {
    (void)alignment;
}

void RSX_BindTexture(u32 offset, u32 width, u32 height, u32 stride, u32 format) {
    gcmTexture tex;
    memset(&tex, 0, sizeof(gcmTexture));
    tex.format = format;
    tex.mipmap = 1;
    tex.dimension = GCM_TEXTURE_DIMS_2D;
    tex.width = width;
    tex.height = height;
    tex.depth = 1;
    tex.pitch = stride;
    tex.location = GCM_LOCATION_RSX;
    tex.offset = offset;
    rsxLoadTexture(context, globalTextureUnit_id, &tex);
    rsxTextureControl(context, globalTextureUnit_id, GCM_TRUE, 0, 15 << 8, GCM_TEXTURE_MAX_ANISO_1);
    rsxTextureFilter(context, globalTextureUnit_id, GCM_TEXTURE_LINEAR, GCM_TEXTURE_LINEAR, GCM_TEXTURE_CONVOLUTION_QUINCUNX);
}

void RSX_UpdateScreen() {
    rsxFinish(context, 0);
    flip();
}

void RSX_GetVideoConfig(uint32_t *internal_w, uint32_t *internal_h,
                         uint32_t *display_w, uint32_t *display_h) {
    if (internal_w) *internal_w = g_video_config.internal_w;
    if (internal_h) *internal_h = g_video_config.internal_h;
    if (display_w) *display_w = g_video_config.display_w;
    if (display_h) *display_h = g_video_config.display_h;
}
