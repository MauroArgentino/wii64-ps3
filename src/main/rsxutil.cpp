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

static void waitFinish()
{
	rsxSetWriteBackendLabel(context,GCM_LABEL_INDEX,sLabelVal);

	rsxFlushBuffer(context);

	while(*(vu32*)gcmGetLabelAddress(GCM_LABEL_INDEX)!=sLabelVal)
		usleep(30);

	++sLabelVal;
}

static void waitRSXIdle()
{
	rsxSetWriteBackendLabel(context,GCM_LABEL_INDEX,sLabelVal);
	rsxSetWaitLabel(context,GCM_LABEL_INDEX,sLabelVal);

	++sLabelVal;

	waitFinish();
}

void setRenderTarget(u32 index)
{
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

	sf.depthFormat		= GCM_TF_ZETA_Z16;
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

void init_screen(void *host_addr,u32 size)
{
	context = rsxInit(CB_SIZE,size,host_addr);

	videoState state;
	videoGetState(0,0,&state);

	videoConfiguration vconfig;
	memset(&vconfig,0,sizeof(videoConfiguration));

	videoGetResolution(state.displayMode.resolution,&res);
	
	videoResolution res_1080;
	// Attempt to set 1920x1080 resolution for the menu.
	if (videoGetResolution(VIDEO_RESOLUTION_1080, &res_1080) == 0) {
		// 1080p is supported, use it.
		vconfig.resolution = VIDEO_RESOLUTION_1080;
		display_width = 1920;
		display_height = 1080;
	} else {
		// 1080p is not supported, fall back to the detected resolution
		vconfig.resolution = state.displayMode.resolution;
		display_width = res.width;
		display_height = res.height;
	}

	vconfig.format = VIDEO_BUFFER_FORMAT_XRGB;
	vconfig.pitch = display_width*sizeof(u32);

	waitRSXIdle();

	videoConfigure(0,&vconfig,NULL,0);
	videoGetState(0,0,&state);

	gcmSetFlipMode(GCM_FLIP_VSYNC);

	color_pitch = display_width*sizeof(u32);
	color_buffer[0] = (u32*)rsxMemalign(64,(display_height*color_pitch));
	color_buffer[1] = (u32*)rsxMemalign(64,(display_height*color_pitch));

	rsxAddressToOffset(color_buffer[0],&color_offset[0]);
	rsxAddressToOffset(color_buffer[1],&color_offset[1]);

	gcmSetDisplayBuffer(0,color_offset[0],color_pitch,display_width,display_height);
	gcmSetDisplayBuffer(1,color_offset[1],color_pitch,display_width,display_height);

	depth_pitch = display_width*sizeof(u32);
	depth_buffer = (u32*)rsxMemalign(64,(display_height*depth_pitch)*2);
	rsxAddressToOffset(depth_buffer,&depth_offset);
}

void waitflip()
{
	while(gcmGetFlipStatus()!=0)
		usleep(200);
	gcmResetFlipStatus();
}

void flip()
{
	if(!first_fb) waitflip();
	else gcmResetFlipStatus();

	gcmSetFlip(context,curr_fb);
	gcmSetWaitFlip(context);

	curr_fb ^= 1;
	setRenderTarget(curr_fb);

	rsxSetClearColor(context, 0);
	rsxClearSurface(context, GCM_CLEAR_R | GCM_CLEAR_G | GCM_CLEAR_B |
	                         GCM_CLEAR_A | GCM_CLEAR_S | GCM_CLEAR_Z);

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

void rsxSetAlphaTestEnable(gcmContextData *ctx, u32 enable)
{
	RSX_CTX_BEGIN(ctx, 2);
	RSX_CTX_PTR(ctx)[0] = RSX_METHOD_ALPHA(NV40TCL_ALPHA_TEST_ENABLE);
	RSX_CTX_PTR(ctx)[1] = enable;
	RSX_CTX_END(ctx, 2);
}

void rsxSetAlphaTestFunc(gcmContextData *ctx, u32 func)
{
	RSX_CTX_BEGIN(ctx, 2);
	RSX_CTX_PTR(ctx)[0] = RSX_METHOD_ALPHA(NV40TCL_ALPHA_TEST_FUNC);
	RSX_CTX_PTR(ctx)[1] = func;
	RSX_CTX_END(ctx, 2);
}

void rsxSetAlphaTestRef(gcmContextData *ctx, u32 ref)
{
	RSX_CTX_BEGIN(ctx, 2);
	RSX_CTX_PTR(ctx)[0] = RSX_METHOD_ALPHA(NV40TCL_ALPHA_TEST_REF);
	RSX_CTX_PTR(ctx)[1] = ref;
	RSX_CTX_END(ctx, 2);
}
