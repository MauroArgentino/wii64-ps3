/**
 * Wii64 - GraphicsRSX.cpp
 * Copyright (C) 2009 sepp256
 *
 * Wii64 homepage: http://www.emulatemii.com
 * email address: sepp256@gmail.com
 *
 *
 * This program is free software; you can redistribute it and/
 * or modify it under the terms of the GNU General Public Li-
 * cence as published by the Free Software Foundation; either
 * version 2 of the Licence, or any later version.
 *
 * This program is distributed in the hope that it will be use-
 * ful, but WITHOUT ANY WARRANTY; without even the implied war-
 * ranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public Licence for more details.
 *
**/

#include <math.h>
#include "GraphicsRSX.h"
#include "../../main/wii64config.h"
#ifdef HW_RVL
#include "../gc_memory/MEM2.h"
#endif

#define DEFAULT_FIFO_SIZE		(256 * 1024)

#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

// display_width/display_height are now macros in rsxutil.h
// (Video_Resolution.width/height from Tiny3D)
s32 globalTextureUnit_id;
s32 vertexPosition_id;
s32 vertexColor0_id;
s32 vertexTexcoord_id;

extern "C" unsigned int usleep(unsigned int us);
void video_mode_init(GXRModeObj *rmode, unsigned int *fb1, unsigned int *fb2);

namespace menu {

Graphics::Graphics(GXRModeObj *rmode)
		: which_fb(0),
		  first_frame(true),
		  depth(1.0f),
		  transparency(1.0f),
		  viewportWidth((float)display_width),
		  viewportHeight((float)display_height)
{
//	printf("Graphics constructor\n");

	fp_buffer = NULL;
	shader_mode = SHADER_PASSCOLOR;

	setColor((GXColor) {0,0,0,0});
	init();
}

Graphics::~Graphics()
{
	if (fp_buffer)
		rsxFree(fp_buffer);
}

void Graphics::init()
{
	//Setup textures, arrays, matrices...
//	f32 aspect_ratio = 4.0f/3.0f;

	fpsize = 0;
	projMatrix_id = -1;
	modelViewMatrix_id = -1;
	vertexPosition_id = -1;
	vertexColor0_id = -1;
	vertexTexcoord_id = -1;
	textureUnit_id = -1;
	mode_id = -1;
	vp_ucode = NULL;
	fp_ucode = NULL;

	Point3 eye_pos = Point3(0.0f,0.0f,20.0f);
	Point3 eye_dir = Point3(0.0f,0.0f,0.0f);
	Vector3 up_vec = Vector3(0.0f,1.0f,0.0f);

//	modelViewMatrix = Matrix4::lookAt(eye_pos,eye_dir,up_vec) * Matrix4::identity();
	modelViewMatrix = Matrix4::identity();
	// Usamos proyeccion logica fija de 640x480 para mantener consistencia con GraphicsGX
	projMatrix = transpose(Matrix4::orthographic(0.0f, 640.0f, 480.0f, 0.0f, 0.0f, 700.0f ));

	vpo = (rsxVertexProgram*)combined_shader_vpo;
	fpo = (rsxFragmentProgram*)combined_shader_fpo;

	vp_ucode = rsxVertexProgramGetUCode(vpo);
	projMatrix_id = rsxVertexProgramGetConst(vpo,"projMatrix");
	modelViewMatrix_id = rsxVertexProgramGetConst(vpo,"modelViewMatrix");
	vertexPosition_id = rsxVertexProgramGetAttrib(vpo,"vertexPosition");
	::vertexPosition_id = vertexPosition_id;
	vertexColor0_id = rsxVertexProgramGetAttrib(vpo,"vertexColor");
	::vertexColor0_id = vertexColor0_id;
	vertexTexcoord_id = rsxVertexProgramGetAttrib(vpo,"vertexTexcoord");
	::vertexTexcoord_id = vertexTexcoord_id;

	fp_ucode = rsxFragmentProgramGetUCode(fpo,&fpsize);
	fp_buffer = (u32*)rsxMemalign(64,fpsize);
	memcpy(fp_buffer,fp_ucode,fpsize);
	rsxAddressToOffset(fp_buffer,&fp_offset);

	mode_id = rsxFragmentProgramGetConst(fpo,"mode");
	textureUnit_id = rsxFragmentProgramGetAttrib(fpo,"texture");
	globalTextureUnit_id = textureUnit_id;
}

void Graphics::drawInit()
{
	//dbg_printf("Graphics drawInit\r\n");
	//init_shader();
	setTEV(GX_PASSCLR);


	rsxInvalidateTextureCache(context,GCM_INVALIDATE_TEXTURE);

	//setup draw environment:
	rsxSetColorMask(context,GCM_COLOR_MASK_B |
							GCM_COLOR_MASK_G |
							GCM_COLOR_MASK_R |
							GCM_COLOR_MASK_A);
	rsxSetColorMaskMRT(context,0);

	u16 x,y,w,h;
	f32 min, max;
	f32 scale[4],offset[4];

	x = 0;
	y = 0;
	w = display_width;
	h = display_height;
	min = 0.0f;
	max = 1.0f;
	scale[0] = w*0.5f;
	scale[1] = h*-0.5f;
	scale[2] = (max - min)*0.5f;
	scale[3] = 0.0f;
	offset[0] = x + w*0.5f;
	offset[1] = y + h*0.5f;
	offset[2] = (max + min)*0.5f;
	offset[3] = 0.0f;

	rsxSetViewport(context,x, y, w, h, min, max, scale, offset);
	rsxSetScissor(context,x,y,w,h);

	rsxSetDepthTestEnable(context,GCM_TRUE);
	// Cambiamos GCM_ALWAYS por GCM_LEQUAL (Menor o Igual) que es el estándar 3D.
	// Esto permite que el Z-Buffer funcione correctamente para el juego.
	rsxSetDepthFunc(context,GCM_LEQUAL);
	rsxSetShadeModel(context,GCM_SHADE_MODEL_SMOOTH);
	rsxSetDepthWriteEnable(context,1);
	rsxSetFrontFace(context,GCM_FRONTFACE_CCW);
	rsxSetCullFace(context,GCM_CULL_BACK);
	rsxSetCullFaceEnable(context,GCM_FALSE);

	//Clear color buffer
	u32 color = 0;
	rsxSetClearColor(context,color);
	rsxSetClearDepthValue(context,0xffff);
	rsxClearSurface(context,GCM_CLEAR_R |
							GCM_CLEAR_G |
							GCM_CLEAR_B |
							GCM_CLEAR_A |
							GCM_CLEAR_Z | GCM_CLEAR_S); // Limpiar también Stencil

	rsxZControl(context,0,1,1);

	for(int i=0;i<8;i++)
		rsxSetViewportClip(context,i,display_width,display_height);

	rsxSetUserClipPlaneControl(context,GCM_USER_CLIP_PLANE_DISABLE,
									   GCM_USER_CLIP_PLANE_DISABLE,
									   GCM_USER_CLIP_PLANE_DISABLE,
									   GCM_USER_CLIP_PLANE_DISABLE,
									   GCM_USER_CLIP_PLANE_DISABLE,
									   GCM_USER_CLIP_PLANE_DISABLE);

	// Por defecto desactivamos Blending (como en drawInit de GraphicsGX)
	rsxSetBlendEnable(context, GCM_FALSE);
	rsxSetBlendFunc(context, GCM_ONE, GCM_ZERO, GCM_ONE, GCM_ZERO);

	//Load Vertex and Fragment Programs
	rsxLoadVertexProgram(context,vpo,vp_ucode);
	rsxSetVertexProgramParameter(context,vpo,projMatrix_id,(float*)&projMatrix);
	rsxSetVertexProgramParameter(context,vpo,modelViewMatrix_id,(float*)&modelViewMatrix);

	rsxSetFragmentProgramParameter(context,fpo,mode_id,&shader_mode,fp_offset);
	rsxLoadFragmentProgramLocation(context,fpo,fp_offset,GCM_LOCATION_RSX);
}

void Graphics::swapBuffers()
{
	flip();
}

void Graphics::clearEFB(GXColor color, u32 zvalue)
{
	//Clear color buffer
	u32 color32 = ((u32)color.r<<16)|((u32)color.g<<8)|(u32)color.b; // Asumiendo formato XRGB (0x00RRGGBB)
	rsxSetClearColor(context,color32);
	rsxSetClearDepthValue(context,zvalue);
	rsxClearSurface(context,GCM_CLEAR_R |
							GCM_CLEAR_G |
							GCM_CLEAR_B |
							GCM_CLEAR_A |
							GCM_CLEAR_S |
							GCM_CLEAR_Z);
}

void Graphics::newModelView()
{
	modelViewMatrix = Matrix4::identity();
}

void Graphics::translate(float x, float y, float z)
{
	modelViewMatrix = modelViewMatrix * Matrix4::translation(Vector3(x, y, z));
}

void Graphics::translateApply(float x, float y, float z)
{
	modelViewMatrix = modelViewMatrix * Matrix4::translation(Vector3(x, y, z));
}

void Graphics::rotate(float degrees)
{
	modelViewMatrix = modelViewMatrix * Matrix4::rotationZ(degrees * (M_PI / 180.0f));
}

void Graphics::loadModelView()
{
	if (vpo && modelViewMatrix_id != -1) {
		rsxSetVertexProgramParameter(context, vpo, modelViewMatrix_id, (float*)&modelViewMatrix);
	}
}

void Graphics::loadPerspective(float fovy, float aspect, float near, float far)
{
	// Generamos la matriz de perspectiva para el mundo 3D de la N64
	projMatrix = transpose(Matrix4::perspective(fovy * (M_PI / 180.0f), aspect, near, far));
	if (vpo && projMatrix_id != -1) {
		rsxSetVertexProgramParameter(context, vpo, projMatrix_id, (float*)&projMatrix);
	}
}

void Graphics::loadOrthographic()
{
	if (vpo && projMatrix_id != -1) {
		rsxSetVertexProgramParameter(context, vpo, projMatrix_id, (float*)&projMatrix);
	}
}

void Graphics::setDepth(float newDepth)
{
	depth = newDepth;
}

float Graphics::getDepth()
{
	return depth;
}

void Graphics::setColor(GXColor color)
{
	for (int i = 0; i < 4; i++){
		currentColor[i].r = color.r;
		currentColor[i].g = color.g;
		currentColor[i].b = color.b;
		currentColor[i].a = color.a;
	}
	applyCurrentColor();
}

void Graphics::setColor(GXColor* color)
{
	for (int i = 0; i < 4; i++){
		currentColor[i].r = color[i].r;
		currentColor[i].g = color[i].g;
		currentColor[i].b = color[i].b;
		currentColor[i].a = color[i].a;
	}
	applyCurrentColor();
}

void Graphics::drawRect(int x, int y, int width, int height)
{
	rsxDrawVertexBegin(context,GCM_TYPE_LINE_STRIP);
		rsxDrawVertex4f(context, vertexColor0_id, (float)appliedColor[0].r/255.0f, (float)appliedColor[0].g/255.0f, 
			(float)appliedColor[0].b/255.0f, (float)appliedColor[0].a/255.0f);
		rsxDrawVertex2f(context, vertexTexcoord_id, 0.0f,0.0f);
		rsxDrawVertex4f(context, vertexPosition_id, (float) x,(float) y, depth, 1.0f);

		rsxDrawVertex4f(context, vertexColor0_id, (float)appliedColor[1].r/255.0f, (float)appliedColor[1].g/255.0f, 
			(float)appliedColor[1].b/255.0f, (float)appliedColor[1].a/255.0f);
		rsxDrawVertex2f(context, vertexTexcoord_id, 0.0f,0.0f);
		rsxDrawVertex4f(context, vertexPosition_id, (float) (x+width),(float) y, depth, 1.0f);

		rsxDrawVertex4f(context, vertexColor0_id, (float)appliedColor[2].r/255.0f, (float)appliedColor[2].g/255.0f, 
			(float)appliedColor[2].b/255.0f, (float)appliedColor[2].a/255.0f);
		rsxDrawVertex2f(context, vertexTexcoord_id, 0.0f,0.0f);
		rsxDrawVertex4f(context, vertexPosition_id, (float) (x+width),(float) (y+height), depth, 1.0f);

		rsxDrawVertex4f(context, vertexColor0_id, (float)appliedColor[3].r/255.0f, (float)appliedColor[3].g/255.0f, 
			(float)appliedColor[3].b/255.0f, (float)appliedColor[3].a/255.0f);
		rsxDrawVertex2f(context, vertexTexcoord_id, 0.0f,0.0f);
		rsxDrawVertex4f(context, vertexPosition_id, (float) x,(float) (y+height), depth, 1.0f);
	rsxDrawVertexEnd(context);
}

void Graphics::fillRect(int x, int y, int width, int height)
{
//	dbg_printf("fillRect x %d, y %d, wd %d, ht %d, dpth %d, Col %d, %d, %d, %d\r\n", x, y, width, height, depth, 
//		appliedColor[0].r, appliedColor[0].g, appliedColor[0].b, appliedColor[0].a);

	rsxDrawVertexBegin(context,GCM_TYPE_QUADS);
		rsxDrawVertex4f(context, vertexColor0_id, (float)appliedColor[0].r/255.0f, (float)appliedColor[0].g/255.0f, 
			(float)appliedColor[0].b/255.0f, (float)appliedColor[0].a/255.0f);
		rsxDrawVertex2f(context, vertexTexcoord_id, 0.0f,0.0f);
		rsxDrawVertex4f(context, vertexPosition_id, (float) x,(float) y, depth, 1.0f);

		rsxDrawVertex4f(context, vertexColor0_id, (float)appliedColor[1].r/255.0f, (float)appliedColor[1].g/255.0f, 
			(float)appliedColor[1].b/255.0f, (float)appliedColor[1].a/255.0f);
		rsxDrawVertex2f(context, vertexTexcoord_id, 0.0f,0.0f);
		rsxDrawVertex4f(context, vertexPosition_id, (float) (x+width),(float) y, depth, 1.0f);

		rsxDrawVertex4f(context, vertexColor0_id, (float)appliedColor[2].r/255.0f, (float)appliedColor[2].g/255.0f, 
			(float)appliedColor[2].b/255.0f, (float)appliedColor[2].a/255.0f);
		rsxDrawVertex2f(context, vertexTexcoord_id, 0.0f,0.0f);
		rsxDrawVertex4f(context, vertexPosition_id, (float) (x+width),(float) (y+height), depth, 1.0f);

		rsxDrawVertex4f(context, vertexColor0_id, (float)appliedColor[3].r/255.0f, (float)appliedColor[3].g/255.0f, 
			(float)appliedColor[3].b/255.0f, (float)appliedColor[3].a/255.0f);
		rsxDrawVertex2f(context, vertexTexcoord_id, 0.0f,0.0f);
		rsxDrawVertex4f(context, vertexPosition_id, (float) x,(float) (y+height), depth, 1.0f);
	rsxDrawVertexEnd(context);
}

void Graphics::drawImage(int textureId, int x, int y, int width, int height, float s1, float s2, float t1, float t2)
{
	//input position and tex coords are measured from top left of screen/texture
	rsxDrawVertexBegin(context,GCM_TYPE_QUADS);
		rsxDrawVertex4f(context, vertexColor0_id, (float)appliedColor[0].r/255.0f, (float)appliedColor[0].g/255.0f, 
			(float)appliedColor[0].b/255.0f, (float)appliedColor[0].a/255.0f);
		rsxDrawVertex2f(context, vertexTexcoord_id, s1, t1);
//		rsxDrawVertex2f(context, vertexTexcoord_id, s2, t1);
//		rsxDrawVertex2f(context, vertexTexcoord_id, s1, t1);
		rsxDrawVertex4f(context, vertexPosition_id, (float) x,(float) y, depth, 1.0f);

		rsxDrawVertex4f(context, vertexColor0_id, (float)appliedColor[1].r/255.0f, (float)appliedColor[1].g/255.0f, 
			(float)appliedColor[1].b/255.0f, (float)appliedColor[1].a/255.0f);
		rsxDrawVertex2f(context, vertexTexcoord_id, s2, t1);
//		rsxDrawVertex2f(context, vertexTexcoord_id, s2, t2);
//		rsxDrawVertex2f(context, vertexTexcoord_id, s2, t1);
		rsxDrawVertex4f(context, vertexPosition_id, (float) (x+width),(float) y, depth, 1.0f);

		rsxDrawVertex4f(context, vertexColor0_id, (float)appliedColor[2].r/255.0f, (float)appliedColor[2].g/255.0f, 
			(float)appliedColor[2].b/255.0f, (float)appliedColor[2].a/255.0f);
		rsxDrawVertex2f(context, vertexTexcoord_id, s2, t2);
//		rsxDrawVertex2f(context, vertexTexcoord_id, s1, t2);
//		rsxDrawVertex2f(context, vertexTexcoord_id, s2, t2);
		rsxDrawVertex4f(context, vertexPosition_id, (float) (x+width),(float) (y+height), depth, 1.0f);

		rsxDrawVertex4f(context, vertexColor0_id, (float)appliedColor[3].r/255.0f, (float)appliedColor[3].g/255.0f, 
			(float)appliedColor[3].b/255.0f, (float)appliedColor[3].a/255.0f);
		rsxDrawVertex2f(context, vertexTexcoord_id, s1, t2);
//		rsxDrawVertex2f(context, vertexTexcoord_id, s1, t1);
//		rsxDrawVertex2f(context, vertexTexcoord_id, s1, t2);
		rsxDrawVertex4f(context, vertexPosition_id, (float) x,(float) (y+height), depth, 1.0f);
	rsxDrawVertexEnd(context);
}

void Graphics::testPrim()
{
//	setTEV(GX_PASSCLR);
//	setTEV(GX_REPLACE);
//	setTEV(GX_MODULATE);
	return;
	rsxDrawVertexBegin(context,GCM_TYPE_QUADS);
		rsxDrawVertex2f(context, vertexTexcoord_id, 0, 0);
		rsxDrawVertex4f(context, vertexColor0_id, 1, 0, 0, 1); //red
		rsxDrawVertex4f(context, vertexPosition_id,  100, 300, depth, 1.0f);

		rsxDrawVertex2f(context, vertexTexcoord_id, 0, 1);
		rsxDrawVertex4f(context, vertexColor0_id, 0, 1, 0, 1); //green
		rsxDrawVertex4f(context, vertexPosition_id,  100, 400, depth, 1.0f);

		rsxDrawVertex2f(context, vertexTexcoord_id, 1, 1);
		rsxDrawVertex4f(context, vertexColor0_id, 0, 0, 1, 1); //blue
		rsxDrawVertex4f(context, vertexPosition_id,  200, 400, depth, 1.0f);

		rsxDrawVertex2f(context, vertexTexcoord_id, 1, 0);
		rsxDrawVertex4f(context, vertexColor0_id, 1, 1, 1, 1); //white
		rsxDrawVertex4f(context, vertexPosition_id,  200, 300, depth, 1.0f);
	rsxDrawVertexEnd(context);
}

void Graphics::drawLine(int x1, int y1, int x2, int y2)
{
	rsxDrawVertexBegin(context,GCM_TYPE_LINES);
		rsxDrawVertex4f(context, vertexColor0_id, (float)appliedColor[0].r/255.0f, (float)appliedColor[0].g/255.0f, 
			(float)appliedColor[0].b/255.0f, (float)appliedColor[0].a/255.0f);
		rsxDrawVertex2f(context, vertexTexcoord_id, 0.0f,0.0f);
		rsxDrawVertex4f(context, vertexPosition_id, (float) x1,(float) y1, depth, 1.0f);

		rsxDrawVertex4f(context, vertexColor0_id, (float)appliedColor[1].r/255.0f, (float)appliedColor[1].g/255.0f, 
			(float)appliedColor[1].b/255.0f, (float)appliedColor[1].a/255.0f);
		rsxDrawVertex2f(context, vertexTexcoord_id, 0.0f,0.0f);
		rsxDrawVertex4f(context, vertexPosition_id, (float) x2,(float) y2, depth, 1.0f);
	rsxDrawVertexEnd(context);
}

#ifndef PI
#define PI 3.14159f
#endif

void Graphics::drawCircle(int x, int y, int radius, int numSegments)
{
	float angle, point_x, point_y;

	rsxDrawVertexBegin(context,GCM_TYPE_LINE_STRIP);

	for (int i = 0; i<=numSegments; i++)
	{
		angle = 2*PI * i/numSegments;
		point_x = (float)x + (float)radius * cos( angle );
		point_y = (float)y + (float)radius * sin( angle );

		rsxDrawVertex4f(context, vertexColor0_id, (float)appliedColor[0].r/255.0f, (float)appliedColor[0].g/255.0f, 
			(float)appliedColor[0].b/255.0f, (float)appliedColor[0].a/255.0f);
		rsxDrawVertex2f(context, vertexTexcoord_id, 0.0f,0.0f);
		rsxDrawVertex4f(context, vertexPosition_id, (float) point_x,(float) point_y, depth, 1.0f);
	}

	rsxDrawVertexEnd(context);
}

void Graphics::drawString(int x, int y, std::string str)
{
	//todo
}

void Graphics::drawPoint(int x, int y, int radius)
{
	rsxDrawVertexBegin(context,GCM_TYPE_POINTS);
		rsxDrawVertex4f(context, vertexColor0_id, (float)appliedColor[0].r/255.0f, (float)appliedColor[0].g/255.0f, 
			(float)appliedColor[0].b/255.0f, (float)appliedColor[0].a/255.0f);
		rsxDrawVertex2f(context, vertexTexcoord_id, 0.0f,0.0f);
		rsxDrawVertex4f(context, vertexPosition_id, (float) x,(float) y, depth, 1.0f);
	rsxDrawVertexEnd(context);
}

void Graphics::setLineWidth(int width)
{
}

void Graphics::pushDepth(float d)
{
	depthStack.push(getDepth());
	setDepth(d);
}

void Graphics::popDepth()
{
	depthStack.pop();
	if(depthStack.size() != 0)
	{
		setDepth(depthStack.top());
	}
	else
	{
		setDepth(1.0f);
	}
}

void Graphics::enableScissor(int x, int y, int width, int height)
{
	// Mapeamos las coordenadas logicas 640x480 a la resolucion fisica de la TV
	u16 sx = (u16)(x * display_width / 640.0f);
	u16 sy = (u16)(y * display_height / 480.0f);
	u16 sw = (u16)(width * display_width / 640.0f);
	u16 sh = (u16)(height * display_height / 480.0f);
	rsxSetScissor(context, sx, sy, sw, sh);
}

void Graphics::disableScissor()
{
	rsxSetScissor(context, 0, 0, (u16)display_width, (u16)display_height);
}

void Graphics::enableBlending(bool blend)
{
	rsxSetBlendEnable(context, blend ? GCM_TRUE : GCM_FALSE);

	if (blend)
	{
		rsxSetBlendFunc(context, GCM_SRC_ALPHA, GCM_ONE_MINUS_SRC_ALPHA, GCM_SRC_ALPHA, GCM_ONE_MINUS_SRC_ALPHA);
		rsxSetBlendEnable(context, GCM_TRUE);
	}
	else
	{
		rsxSetBlendFunc(context, GCM_ONE, GCM_ZERO, GCM_ONE, GCM_ZERO);
	}
}

void Graphics::setTEV(int tev_op)
{
	switch (tev_op)
	{
	case GX_REPLACE:
		shader_mode = (float) SHADER_PASSTEX;
		break;
	case GX_PASSCLR:
		shader_mode = (float) SHADER_PASSCOLOR;
		break;
	case GX_MODULATE:
	default:
		shader_mode = (float) SHADER_MODULATE;
		break;
	}
	rsxSetFragmentProgramParameter(context,fpo,mode_id,&shader_mode,fp_offset);
	rsxLoadFragmentProgramLocation(context,fpo,fp_offset,GCM_LOCATION_RSX);
}

void Graphics::pushTransparency(float f)
{
	transparencyStack.push(getTransparency());
	setTransparency(f);
}

void Graphics::popTransparency()
{
	transparencyStack.pop();
	if(transparencyStack.size() != 0)
	{
		setTransparency(transparencyStack.top());
	}
	else
	{
		setTransparency(1.0f);
	}
}

void Graphics::setTransparency(float f)
{
	transparency = f;
	applyCurrentColor();
}

float Graphics::getTransparency()
{
	return transparency;
}

void Graphics::applyCurrentColor()
{
	for (int i = 0; i < 4; i++){
		appliedColor[i].r = currentColor[i].r;
		appliedColor[i].g = currentColor[i].g;
		appliedColor[i].b = currentColor[i].b;
		appliedColor[i].a = (u8) (getCurrentTransparency(i) * 255.0f);
	}
}

float Graphics::getCurrentTransparency(int index)
{
	float alpha = (float)currentColor[index].a/255.0f;
	float val = alpha * transparency;
	return val;
}

} //namespace menu 
