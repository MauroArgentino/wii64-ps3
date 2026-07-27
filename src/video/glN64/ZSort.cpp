/**
 * ZSort.cpp - ZSort microcode for Mario Kart 64
 * Minimal implementation for Z-sorted transparent particles (smoke, sparks, etc.)
 * Based on GLideN64's ZSort logic but simplified for RSX fixed-function pipeline
 */

#include "ZSort.h"
#include "RSP.h"
#include "gDP.h"
#include "gSP.h"
#include "Textures.h"
#include "OpenGL.h"
#include "convert.h"
#include "gDP.cpp"

#ifdef PS3
#include "RSX_VideoBackend.h"
#include <rsx/rsx.h>
#include <rsx/gcm_sys.h>
#include <sys/gcm_sys.h>
#include <sys/gcm.h>
#include <rsx/rsxutil.h>
extern context_t *context;
#endif

// ZSort microcode state
static bool zsort_enabled = false;
static bool zsort_depth_test_enabled = true;

// ZSort command opcodes (from N64 RSP ZSort microcode)
#define G_ZSORT_TRI1      0x80
#define G_ZSORT_TRI2      0x81
#define G_ZSORT_QUAD      0x82
#define G_ZSORT_LOAD_UCODE 0x83
#define G_ZSORT_SET_Z      0x84
#define G_ZSORT_SET_ALPHA  0x85
#define G_ZSORT_SET_DEPTH  0x86

// ZSort state
static struct {
    bool depth_test;
    bool alpha_blend;
    u32 z_value;
    u8 alpha_threshold;
} zsort_state;

void ZSORT_Init()
{
    // Set GeometryMode flags
    GBI_InitFlags( ZSORT );

    GBI.PCStackSize = 18;

    // ZSort commands
    GBI_SetGBI( G_SPNOOP,       ZSORT_SPNOOP,       ZSORT_SPNoOp );
    GBI_SetGBI( G_DL,           ZSORT_DL,           ZSORT_DList );
    GBI_SetGBI( G_TRI1,         ZSORT_TRI1,         ZSORT_Tri1 );
    GBI_SetGBI( G_TRI2,         ZSORT_TRI2,         ZSORT_Tri2 );
    GBI_SetGBI( G_QUAD,         ZSORT_QUAD,         ZSORT_Quad );
    GBI_SetGBI( G_LOAD_UCODE,   ZSORT_LOAD_UCODE,   ZSORT_Load_uCode );
    GBI_SetGBI( G_SETOTHERMODE_H, ZSORT_SETOTHERMODE_H, ZSORT_SetOtherMode_H );
    GBI_SetGBI( G_SETOTHERMODE_L, ZSORT_SETOTHERMODE_L, ZSORT_SetOtherMode_L );
    GBI_SetGBI( G_SETGEOMETRYMODE, ZSORT_SETGEOMETRYMODE, ZSORT_SetGeometryMode );
    GBI_SetGBI( G_CLEARGEOMETRYMODE, ZSORT_CLEARGEOMETRYMODE, ZSORT_ClearGeometryMode );

    // Initialize ZSort state
    zsort_enabled = false;
    zsort_depth_test_enabled = true;
    zsort_state.depth_test = true;
    zsort_state.alpha_blend = false;
    zsort_state.z_value = 0;
    zsort_state.alpha_threshold = 0;
}

void ZSORT_SwitchToZSort()
{
    zsort_enabled = true;
    
#ifdef PS3
    // Enable depth test for Z-sorted particles
    rsxSetDepthTestEnable(context, GCM_TRUE);
    rsxSetDepthFunc(context, GCM_LESS);
    
    // Enable alpha blending for transparent particles
    rsxSetBlendEnable(context, GCM_TRUE);
    rsxSetBlendFunc(context, GCM_SRC_ALPHA, GCM_ONE_MINUS_SRC_ALPHA, GCM_SRC_ALPHA, GCM_ONE_MINUS_SRC_ALPHA);
    rsxSetBlendEquation(context, GCM_FUNC_ADD, GCM_FUNC_ADD);
    
    // Disable depth writes for transparent particles (prevent self-occlusion)
    rsxSetDepthWriteEnable(context, GCM_FALSE);
    
    zsort_state.depth_test = true;
    zsort_state.alpha_blend = true;
#endif
}

void ZSORT_SwitchFromZSort()
{
    zsort_enabled = false;
    
#ifdef PS3
    // Restore normal rendering state
    rsxSetDepthWriteEnable(context, GCM_TRUE);
    rsxSetDepthFunc(context, GCM_LESS);
    rsxSetBlendEnable(context, GCM_FALSE);
    
    zsort_state.depth_test = false;
    zsort_state.alpha_blend = false;
#endif
}

// ZSort command implementations
void ZSORT_SPNoOp( u32 w0, u32 w1 )
{
    // No operation
}

void ZSORT_DList( u32 w0, u32 w1 )
{
    u32 dl = _SHIFTR( w1, 0, 24 );
    u32 push = _SHIFTR( w0, 16, 1 );
    
    if (push)
        gSPBranchList( dl );
    else
        gSPDisplayList( dl );
}

void ZSORT_Tri1( u32 w0, u32 w1 )
{
    // Single triangle with Z-sort
    u32 v0 = _SHIFTR( w1, 0, 8 );
    u32 v1 = _SHIFTR( w1, 8, 8 );
    u32 v2 = _SHIFTR( w1, 16, 8 );
    
    // Enable Z-sort rendering mode
    ZSORT_SwitchToZSort();
    
    // Delegate to standard triangle rendering with Z-sort enabled
    gSP1Triangle( v0, v1, v2, 0 );
    
    ZSORT_SwitchFromZSort();
}

void ZSORT_Tri2( u32 w0, u32 w1 )
{
    // Two triangles with Z-sort
    u32 v0 = _SHIFTR( w1, 0, 8 );
    u32 v1 = _SHIFTR( w1, 8, 8 );
    u32 v2 = _SHIFTR( w1, 16, 8 );
    u32 v3 = _SHIFTR( w1, 24, 8 );
    u32 v4 = _SHIFTR( w0, 0, 8 );
    u32 v5 = _SHIFTR( w0, 8, 8 );
    
    ZSORT_SwitchToZSort();
    gSP2Triangles( v0, v1, v2, 0, v3, v4, v5, 0 );
    ZSORT_SwitchFromZSort();
}

void ZSORT_Quad( u32 w0, u32 w1 )
{
    // Quad with Z-sort (two triangles)
    u32 v0 = _SHIFTR( w1, 0, 8 );
    u32 v1 = _SHIFTR( w1, 8, 8 );
    u32 v2 = _SHIFTR( w1, 16, 8 );
    u32 v3 = _SHIFTR( w1, 24, 8 );
    u32 v4 = _SHIFTR( w0, 0, 8 );
    u32 v5 = _SHIFTR( w0, 8, 8 );
    u32 v6 = _SHIFTR( w0, 16, 8 );
    u32 v7 = _SHIFTR( w0, 24, 8 );
    
    ZSORT_SwitchToZSort();
    
    // Draw as two triangles with Z-sort
    gSP1Triangle( v0, v1, v2, 0 );
    gSP1Triangle( v0, v2, v3, 0 );
    
    ZSORT_SwitchFromZSort();
}

void ZSORT_Load_uCode( u32 w0, u32 w1 )
{
    gSPLoadUcodeEx( w1, gDP.half_1, _SHIFTR( w0, 0, 16 ) + 1 );
}

void ZSORT_SetOtherMode_H( u32 w0, u32 w1 )
{
    gDPSetOtherModeH( w0, w1 );
}

void ZSORT_SetOtherMode_L( u32 w0, u32 w1 )
{
    gDPSetOtherModeL( w0, w1 );
}

void ZSORT_SetGeometryMode( u32 w0, u32 w1 )
{
    gSPSetGeometryMode( w0, w1 );
}

void ZSORT_ClearGeometryMode( u32 w0, u32 w1 )
{
    gSPClearGeometryMode( w0, w1 );
}

void ZSORT_SPNoOp( u32 w0, u32 w1 )
{
    // No operation
}

void ZSORT_DList( u32 w0, u32 w1 )
{
    u32 dl = _SHIFTR( w1, 0, 24 );
    u32 push = _SHIFTR( w0, 16, 1 );
    
    if (push)
        gSPBranchList( dl );
    else
        gSPDisplayList( dl );
}

void ZSORT_DList_Push( u32 w0, u32 w1 )
{
    // Not used
}

void ZSORT_DList_NoPush( u32 w0, u32 w1 )
{
    // Not used
}