/**
 * RSX Video Backend Bridge for glN64
 * Translates N64 RDP commands to PS3 RSX calls.
 */

#include <rsx/rsx.h>
#include "../../ui/libgui/GraphicsRSX.h"
#include "../../main/rsxutil.h"

extern gcmContextData *context;
extern s32 globalTextureUnit_id;

// IDs de atributos del shader (deben coincidir con GraphicsRSX)
extern s32 vertexPosition_id;
extern s32 vertexColor0_id;
extern s32 vertexTexcoord_id;

/**
 * Reemplazo para glBegin(GL_TRIANGLES) / GX_Begin(GX_TRIANGLES)
 * Renderiza un triángulo de la N64 usando el pipeline de la PS3.
 */
extern "C" void RSX_DrawTriangle(float* v0, float* v1, float* v2, bool texEnabled)
{
    // vX: [x, y, z, r, g, b, a, s, t]
    rsxDrawVertexBegin(context, GCM_TYPE_TRIANGLES);

    // Vertice 0
    rsxDrawVertex2f(context, vertexTexcoord_id, v0[7], v0[8]);
    rsxDrawVertex4f(context, vertexColor0_id, v0[3], v0[4], v0[5], v0[6]);
    rsxDrawVertex4f(context, vertexPosition_id, v0[0], v0[1], v0[2], 1.0f);

    // Vertice 1
    rsxDrawVertex2f(context, vertexTexcoord_id, v1[7], v1[8]);
    rsxDrawVertex4f(context, vertexColor0_id, v1[3], v1[4], v1[5], v1[6]);
    rsxDrawVertex4f(context, vertexPosition_id, v1[0], v1[1], v1[2], 1.0f);

    // Vertice 2
    rsxDrawVertex2f(context, vertexTexcoord_id, v2[7], v2[8]);
    rsxDrawVertex4f(context, vertexColor0_id, v2[3], v2[4], v2[5], v2[6]);
    rsxDrawVertex4f(context, vertexPosition_id, v2[0], v2[1], v2[2], 1.0f);

    rsxDrawVertexEnd(context);
}

/**
 * Reemplazo para el manejo de texturas (glTexImage2D / GX_LoadTexObj)
 */
extern "C" void RSX_BindTexture(u32 offset, u32 width, u32 height, u32 stride, u32 format)
{
    gcmTexture tex;
    memset(&tex, 0, sizeof(gcmTexture));
    tex.format = format; // Ej: GCM_TEXTURE_FORMAT_A8R8G8B8
    tex.mipmap = 1;
    tex.dimension = GCM_TEXTURE_DIMS_2D; // Asegurarse de que esta constante es correcta
    tex.width = width;
    tex.height = height;
    tex.depth = 1;
    tex.pitch = stride;
    tex.location = GCM_LOCATION_RSX;
    tex.offset = offset;
    rsxLoadTexture(context, globalTextureUnit_id, &tex);
    rsxTextureControl(context, globalTextureUnit_id, GCM_TRUE, 0, 15 << 8, GCM_TEXTURE_MAX_ANISO_1); // Asegurarse de que esta constante es correcta
    rsxTextureFilter(context, globalTextureUnit_id, GCM_TEXTURE_LINEAR, GCM_TEXTURE_LINEAR, GCM_TEXTURE_CONVOLUTION_QUINCUNX);
}

/**
 * Presentación de Frame (Reemplazo para swapBuffers/glFlush)
 */
extern "C" void RSX_UpdateScreen()
{
    // En PS3 debemos esperar a que la GPU termine antes de hacer el flip
    // para evitar el "tearing" o parpadeos negros.
    rsxFinish(context, 0);
    flip();
}