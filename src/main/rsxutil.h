#ifndef __RSXUTIL_H__
#define __RSXUTIL_H__

#include <ppu-types.h>

#define CB_SIZE		0x100000
#define HOST_SIZE	(64*1024*1024)

extern gcmContextData *context;
extern u32 display_width;
extern u32 display_height;
extern u32 curr_fb;
extern u32 first_fb;

extern u32 depth_pitch;
extern u32 depth_offset;
extern u32 *depth_buffer;

extern u32 color_pitch;
extern u32 color_offset[2];
extern u32 *color_buffer[2];

extern int rsx_hung;

typedef struct {
	uint32_t internal_w, internal_h;
	uint32_t display_w, display_h;
	uint32_t initialized;
	void    *fb_addr[2];
	uint32_t fb_offset[2];
	uint32_t fb_width, fb_height, fb_pitch;
} VideoConfig;

extern VideoConfig g_video_config;

#ifdef __cplusplus
extern "C" {
#endif

void setRenderTarget(u32 index);
void init_screen(void *host_addr,u32 size);
void waitflip();
void flip();

void rsxSetAlphaTestEnable(gcmContextData *ctx, u32 enable);
void rsxSetAlphaTestFunc(gcmContextData *ctx, u32 func);
void rsxSetAlphaTestRef(gcmContextData *ctx, u32 ref);

void RSX_ConfigureViewport(uint32_t internal_w, uint32_t internal_h,
                           uint32_t display_w, uint32_t display_h);
void RSX_SetInternalResolution(uint32_t w, uint32_t h);
void RSX_SetDisplayResolution(uint32_t w, uint32_t h);
void RSX_ApplyConfigResolution(void);

#ifdef __cplusplus
}
#endif

#endif