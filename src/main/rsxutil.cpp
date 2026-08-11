#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <malloc.h>
#include <ppu-types.h>

#include <rsx/rsx.h>
#include <rsx/nv40.h>
#include <sysutil/video.h>

#include "rsxutil.h"
#include "../main/wii64config.h"

#define GCM_LABEL_INDEX		255

videoResolution res;
gcmContextData *context = NULL;

u32 curr_fb = 0;
u32 first_fb = 1;

u32 display_width;
u32 display_height;

u32 depth_pitch;
u32 depth_offset;
u32 *depth_buffer;

u32 color_pitch;
u32 color_offset[2];
u32 *color_buffer[2];

static u32 sLabelVal = 1;
int rsx_hung = 0;

VideoConfig g_video_config;

static void waitFinish() {
	u32 timeout = 0;
	rsxSetWriteBackendLabel(context,GCM_LABEL_INDEX,sLabelVal);

	rsxFlushBuffer(context);

	while(*(vu32*)gcmGetLabelAddress(GCM_LABEL_INDEX)!=sLabelVal) {
		usleep(30);
		if(++timeout > 100000) {
			rsx_hung = 1;
			return;
		}
	}

	++sLabelVal;
}

static void waitRSXIdle() {
	if (rsx_hung) return;

	rsxSetWriteBackendLabel(context,GCM_LABEL_INDEX,sLabelVal);
	rsxSetWaitLabel(context,GCM_LABEL_INDEX,sLabelVal);

	++sLabelVal;

	waitFinish();
}

void setRenderTarget(u32 index) {
	gcmSurface sf;

	sf.colorFormat		= GCM_TF_COLOR_X8R8G8B8;
	sf.colorTarget		= GCM_TF_TARGET_0;
	sf.colorLocation[0]	= GCM_LOCATION_RSX;
	sf.colorOffset[0]	= color_offset[index];
	sf.colorPitch[0]	= color_pitch;

	sf.colorLocation[1]	= GCM_LOCATION_RSX;
	sf.colorLocation[2]	= GCM_LOCATION_RSX;
	sf.colorLocation[3]	= GCM_LOCATION_RSX;
	sf.colorOffset[1]	= 0;
	sf.colorOffset[2]	= 0;
	sf.colorOffset[3]	= 0;
	sf.colorPitch[1]	= 64;
	sf.colorPitch[2]	= 64;
	sf.colorPitch[3]	= 64;

	sf.depthFormat		= GCM_TF_ZETA_Z24S8;
	sf.depthLocation	= GCM_LOCATION_RSX;
	sf.depthOffset		= depth_offset;
	sf.depthPitch		= depth_pitch;

	sf.type				= GCM_TF_TYPE_LINEAR;
	sf.antiAlias		= GCM_TF_CENTER_1;

	sf.width			= display_width;
	sf.height			= display_height;
	sf.x				= 0;
	sf.y				= 0;

	rsxSetSurface(context,&sf);
}

void init_screen(void *host_addr,u32 size) {
	context = rsxInit(CB_SIZE,size,host_addr);
	if (context == NULL) {
		if (host_addr) free(host_addr);
		return;
	}

	videoState state;
	if (videoGetState(0,0,&state) != 0) {
		rsxFinish(context,0);
		if (host_addr) free(host_addr);
		context = NULL;
		return;
	}

	/* Make sure display is enabled */
	if (state.state != 0) {
		rsxFinish(context,0);
		if (host_addr) free(host_addr);
		context = NULL;
		return;
	}

	videoConfiguration vconfig;
	memset(&vconfig,0,sizeof(videoConfiguration));

	videoGetResolution(state.displayMode.resolution,&res);
	
	// Selecciona la resolucion interna segun settings.cfg (vidResolution).
	// El modo de salida (vconfig.resolution) acompana al interno para evitar
	// escalados raros; si el hardware no soporta el modo pedido, se cae al
	// mejor modo detectado (normalmente 1080p o xdetectado).
	int target_res_id;
	switch (vidResolution) {
		case RESOLUTION_320X240: target_res_id = VIDEO_RESOLUTION_480; break;
		case RESOLUTION_640X480: target_res_id = VIDEO_RESOLUTION_480; break;
		case RESOLUTION_720P:    target_res_id = VIDEO_RESOLUTION_720; break;
		default:                 target_res_id = VIDEO_RESOLUTION_1080; break;
	}
	videoResolution res_target;
	if (videoGetResolution(target_res_id, &res_target) == 0) {
		vconfig.resolution = target_res_id;
	} else if (videoGetResolution(VIDEO_RESOLUTION_1080, &res_target) == 0) {
		vconfig.resolution = VIDEO_RESOLUTION_1080;
		res_target = res;
	} else {
		vconfig.resolution = state.displayMode.resolution;
		res_target = res;
	}

	switch (vidResolution) {
		case RESOLUTION_320X240: display_width = 320;  display_height = 240;  break;
		case RESOLUTION_640X480: display_width = 640;  display_height = 480;  break;
		case RESOLUTION_720P:    display_width = 1280; display_height = 720;  break;
		default:                 display_width = 1920; display_height = 1080; break;
	}

	vconfig.format = VIDEO_BUFFER_FORMAT_XRGB;
	vconfig.pitch = display_width*sizeof(u32);
	vconfig.aspect = state.displayMode.aspect;

	waitRSXIdle();

	if (videoConfigure(0,&vconfig,NULL,0) != 0) {
		rsxFinish(context,0);
		if (host_addr) free(host_addr);
		context = NULL;
		return;
	}
	videoGetState(0,0,&state);

	gcmSetFlipMode(GCM_FLIP_VSYNC);

	color_pitch = display_width*sizeof(u32);
	color_buffer[0] = (u32*)rsxMemalign(64,(display_height*color_pitch));
	color_buffer[1] = (u32*)rsxMemalign(64,(display_height*color_pitch));

	if (!color_buffer[0] || !color_buffer[1]) {
		rsxFinish(context,0);
		if (host_addr) free(host_addr);
		context = NULL;
		return;
	}

	rsxAddressToOffset(color_buffer[0],&color_offset[0]);
	rsxAddressToOffset(color_buffer[1],&color_offset[1]);

	gcmSetDisplayBuffer(0,color_offset[0],color_pitch,display_width,display_height);
	gcmSetDisplayBuffer(1,color_offset[1],color_pitch,display_width,display_height);

	depth_pitch = display_width*sizeof(u32); // Z24S8 = 4 bytes per pixel
	depth_buffer = (u32*)rsxMemalign(64,(display_height*depth_pitch)*2);
	if (!depth_buffer) {
		printf("[RSX] WARN: rsxMemalign failed for depth_buffer (%u bytes) at %ux%u\n",
		       display_height*depth_pitch*2, display_width, display_height);
		depth_offset = 0;
	} else {
		rsxAddressToOffset(depth_buffer, &depth_offset);
	}

	gcmSetDisplayBuffer(0,color_offset[0],color_pitch,display_width,display_height);
	gcmSetDisplayBuffer(1,color_offset[1],color_pitch,display_width,display_height);

	gcmSetFlipMode(GCM_FLIP_VSYNC);

	gcmResetFlipStatus();
}

void waitflip() {
	while(gcmGetFlipStatus()!=0)
		usleep(200);
	gcmResetFlipStatus();
}

void flip() {
	if(!first_fb) waitflip();
	else gcmResetFlipStatus();

	gcmSetFlip(context,curr_fb);
	gcmSetWaitFlip(context);

	curr_fb ^= 1;
	setRenderTarget(curr_fb);

	rsxSetClearColor(context, 0);
	rsxClearSurface(context, GCM_CLEAR_R | GCM_CLEAR_G | GCM_CLEAR_B | GCM_CLEAR_A | GCM_CLEAR_S | GCM_CLEAR_Z);

	rsxFlushBuffer(context);

	first_fb = 0;
}

// Alpha test via raw NV40 register writes
// Replicate internal RSX macros since they are not in public headers
#define RSX_METHOD_ALPHA(method)    (((u32)1 << 18) | (method))
#define RSX_CTX_BEGIN(ctx,n)        do{ if(((ctx)->current+(n))>(ctx)->end) { s32 _r=rsxContextCallback((ctx),(n)); if(_r!=0) return; } }while(0)
#define RSX_CTX_PTR(ctx)            ((ctx)->current)
#define RSX_CTX_END(ctx,n)          (ctx)->current += (n)

extern "C" s32 rsxContextCallback(gcmContextData *context, u32 count);

void rsxSetAlphaTestEnable(gcmContextData *ctx, u32 enable) {
	RSX_CTX_BEGIN(ctx, 2);
	RSX_CTX_PTR(ctx)[0] = RSX_METHOD_ALPHA(NV40TCL_ALPHA_TEST_ENABLE);
	RSX_CTX_PTR(ctx)[1] = enable;
	RSX_CTX_END(ctx, 2);
}

void rsxSetAlphaTestFunc(gcmContextData *ctx, u32 func) {
	RSX_CTX_BEGIN(ctx, 2);
	RSX_CTX_PTR(ctx)[0] = RSX_METHOD_ALPHA(NV40TCL_ALPHA_TEST_FUNC);
	RSX_CTX_PTR(ctx)[1] = func;
	RSX_CTX_END(ctx, 2);
}

void rsxSetAlphaTestRef(gcmContextData *ctx, u32 ref) {
	RSX_CTX_BEGIN(ctx, 2);
	RSX_CTX_PTR(ctx)[0] = RSX_METHOD_ALPHA(NV40TCL_ALPHA_TEST_REF);
	RSX_CTX_PTR(ctx)[1] = ref;
	RSX_CTX_END(ctx, 2);
}

/* Configura el viewport para internal 3D rendering vs display resolution */
void RSX_ConfigureViewport(uint32_t internal_w, uint32_t internal_h,
                           uint32_t display_w, uint32_t display_h) {
    g_video_config.internal_w = internal_w;
    g_video_config.internal_h = internal_h;
    g_video_config.display_w = display_w;
    g_video_config.display_h = display_h;
    display_width = display_w;
    display_height = display_h;
}

void RSX_SetInternalResolution(uint32_t w, uint32_t h) {
    g_video_config.internal_w = w ? w : 640;
    g_video_config.internal_h = h ? h : 480;
}

void RSX_SetDisplayResolution(uint32_t w, uint32_t h) {
    g_video_config.display_w = w ? w : 1920;
    g_video_config.display_h = h ? h : 1080;
}

/* Aplica la resolución interna de render configurada en settings.cfg
   (0=320x240, 1=640x480, 2=720p, 3=1080p). Libera y realoca los
   color/depth buffers al nuevo tamano para ahorrar VRAM, vital para que
   las texturas de framebuffer del glN64 quepan en el heap de 32 MB. */
void RSX_ApplyConfigResolution() {
    uint32_t w, h;
    switch (vidResolution) {
        case RESOLUTION_320X240: w = 320;  h = 240;  break;
        case RESOLUTION_640X480: w = 640;  h = 480;  break;
        case RESOLUTION_720P:    w = 1280; h = 720;  break;
        default:                 w = 1920; h = 1080; break;
    }

    if (w == display_width && h == display_height) return;

    waitRSXIdle();

    if (depth_buffer)  { rsxFree(depth_buffer);  depth_buffer  = NULL; }
    if (color_buffer[0]) { rsxFree(color_buffer[0]); color_buffer[0] = NULL; }
    if (color_buffer[1]) { rsxFree(color_buffer[1]); color_buffer[1] = NULL; }

    display_width = w;
    display_height = h;
    color_pitch = display_width * sizeof(u32);
    depth_pitch = display_width * sizeof(u32);

    printf("[RSX] ApplyConfigResolution: vidResolution=%d -> %ux%u (pitch=%u)\n",
           (int)vidResolution, w, h, color_pitch);

    color_buffer[0] = (u32*)rsxMemalign(64, display_height * color_pitch);
    color_buffer[1] = (u32*)rsxMemalign(64, display_height * color_pitch);
    if (!color_buffer[0] || !color_buffer[1]) {
        printf("[RSX] ERROR: rsxMemalign fallo al realocar color_buffer %ux%u\n", w, h);
        return;
    }
    rsxAddressToOffset(color_buffer[0], &color_offset[0]);
    rsxAddressToOffset(color_buffer[1], &color_offset[1]);

    depth_buffer = (u32*)rsxMemalign(64, display_height * depth_pitch * 2);
    if (!depth_buffer) {
        printf("[RSX] ERROR: rsxMemalign fallo al realocar depth_buffer %ux%u\n", w, h);
    } else {
        rsxAddressToOffset(depth_buffer, &depth_offset);
    }

    gcmSetDisplayBuffer(0, color_offset[0], color_pitch, display_width, display_height);
    gcmSetDisplayBuffer(1, color_offset[1], color_pitch, display_width, display_height);

    setRenderTarget(curr_fb);
    rsxFlushBuffer(context);
}