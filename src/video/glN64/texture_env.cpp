/**
 * glN64_GX - texture_env.cpp
 * Copyright (C) 2003 Orkin
 * Copyright (C) 2008, 2009 sepp256 (Port to Wii/Gamecube/PS3)
 *
 * glN64 homepage: http://gln64.emulation64.com
 * Wii64 homepage: http://www.emulatemii.com
 * email address: sepp256@gmail.com
 *
**/

#ifdef __GX__
#include <gccore.h>
#endif // __GX__

#if !defined(__LINUX__) && !defined(PS3) && !defined(__PPC__)
# include <windows.h>
#else
# include "../../main/winlnxdefs.h"
# include <stdlib.h>
#endif
#include "OpenGL.h"
#include "Combiner.h"
#include "Textures.h"
#include "texture_env.h"
#include "../../main/game_hacks.h"
#include <stdio.h>
#include "../../debug.h"

void Init_texture_env()
{
}

void Uninit_texture_env()
{
}

void Update_texture_env_Colors( TexEnv *texEnv )
{
}

TexEnv *Compile_texture_env( Combiner *color, Combiner *alpha )
{
	TexEnv *texEnv = (TexEnv*)malloc( sizeof( TexEnv ) );

	texEnv->usesT0 = FALSE;
	texEnv->usesT1 = FALSE;
	texEnv->mode = GL_MODULATE;  // safe default — was uninitialized garbage
	texEnv->multiplyByPrimAlpha = FALSE;  // PS3 smoke-fix: N64 2-cycle TEXEL0_ALPHA*PRIMITIVE_ALPHA
	                                          // is dropped to TEXEL0_ALPHA only; this flags primAlpha multiply

	texEnv->fragment.color = texEnv->fragment.alpha = COMBINED;

	for (int i = 0; i < alpha->numStages; i++)
	{
		for (int j = 0; j < alpha->stage[i].numOps; j++)
		{
			switch (alpha->stage[i].op[j].op)
			{
				case LOAD:
					if ((alpha->stage[i].op[j].param1 != TEXEL0_ALPHA) && (alpha->stage[i].op[j].param1 != TEXEL1_ALPHA))
					{
						texEnv->fragment.alpha = alpha->stage[i].op[j].param1;
						texEnv->usesT0 = FALSE;
						texEnv->usesT1 = FALSE;
					}
					else
					{
						texEnv->mode = GL_REPLACE;
						if (gameHacks.mm_fix_logo_alpha)
							texEnv->fragment.alpha = alpha->stage[i].op[j].param1;
						texEnv->usesT0 = alpha->stage[i].op[j].param1 == TEXEL0_ALPHA;
						texEnv->usesT1 = alpha->stage[i].op[j].param1 == TEXEL1_ALPHA;
					}
					break;
				case SUB:
					break;
				case MUL:
					if (((alpha->stage[i].op[j].param1 == TEXEL0_ALPHA) || (alpha->stage[i].op[j].param1 == TEXEL1_ALPHA)) &&
						((alpha->stage[i].op[j - 1].param1 != TEXEL0_ALPHA) || (alpha->stage[i].op[j - 1].param1 != TEXEL1_ALPHA)))
					{
						texEnv->mode = GL_MODULATE;
					}
					else if (((alpha->stage[i].op[j].param1 != TEXEL0_ALPHA) || (alpha->stage[i].op[j].param1 != TEXEL1_ALPHA)) &&
						((alpha->stage[i].op[j - 1].param1 == TEXEL0_ALPHA) || (alpha->stage[i].op[j - 1].param1 == TEXEL1_ALPHA)))
					{
						texEnv->mode = GL_MODULATE;
						if (texEnv->fragment.alpha == TEXEL0_ALPHA || texEnv->fragment.alpha == TEXEL1_ALPHA)
						{
							// Keep texel alpha as source when multiplying by primitive/env/shade
							// texEnv->fragment.alpha already correctly set to TEXEL0/1 from LOAD.
							// N64 2-cycle alpha = TEXEL0_ALPHA * PRIMITIVE_ALPHA (or similar) is
							// approximated as just TEXEL0_ALPHA here. Mark it so the shader multiplies
							// by gDP.primColor.a (PS3 smoke puff fade fix).
							if (alpha->stage[i].op[j].param1 == PRIMITIVE_ALPHA)
								texEnv->multiplyByPrimAlpha = TRUE;
						}
						else
						{
							texEnv->fragment.alpha = alpha->stage[i].op[j].param1;
						}
					}
					else if (texEnv->fragment.alpha != COMBINED)
					{
						// Both operands are non-texel constants (e.g., PRIM * ENV)
						// Propagate the MUL operand as the alpha source
						texEnv->fragment.alpha = alpha->stage[i].op[j].param1;
					}
					break;
				case ADD:
					break;
				case INTER:
					break;
			}
		}
	}

	for (int i = 0; i < color->numStages; i++)
	{
		for (int j = 0; j < color->stage[i].numOps; j++)
		{
			switch (color->stage[i].op[j].op)
			{
				case LOAD:
					if ((color->stage[i].op[j].param1 == TEXEL0) || (color->stage[i].op[j].param1 == TEXEL0_ALPHA))
					{
						if (texEnv->mode == GL_MODULATE)
							texEnv->fragment.color = ONE;

						texEnv->usesT0 = TRUE;
						texEnv->usesT1 = FALSE;
					}
					else if ((color->stage[i].op[j].param1 == TEXEL1) || (color->stage[i].op[j].param1 == TEXEL1_ALPHA))
					{
						if (texEnv->mode == GL_MODULATE)
							texEnv->fragment.color = ONE;

						texEnv->usesT0 = FALSE;
						texEnv->usesT1 = TRUE;
					}
					else
					{
						texEnv->fragment.color = color->stage[i].op[j].param1;
						texEnv->usesT0 = texEnv->usesT1 = FALSE;
					}
					break;
				case SUB:
					break;
				case MUL:
					if ((color->stage[i].op[j].param1 == TEXEL0) || (color->stage[i].op[j].param1 == TEXEL0_ALPHA))
					{
						if (!texEnv->usesT0 && !texEnv->usesT1)
						{
							texEnv->mode = GL_MODULATE;
							texEnv->usesT0 = TRUE;
							texEnv->usesT1 = FALSE;
						}
					}
					else if ((color->stage[i].op[j].param1 == TEXEL1) || (color->stage[i].op[j].param1 == TEXEL1_ALPHA))
					{
						if (!texEnv->usesT0 && !texEnv->usesT1)
						{
							texEnv->mode = GL_MODULATE;
							texEnv->usesT0 = FALSE;
							texEnv->usesT1 = TRUE;
						}
					}
					else if (texEnv->usesT0 || texEnv->usesT1)
					{
						texEnv->mode = GL_MODULATE;
						texEnv->fragment.color = color->stage[i].op[j].param1;
					}
					break;
				case ADD:
					break;
				case INTER:
					if ((color->stage[i].op[j].param1 == TEXEL0) &&
					    ((color->stage[i].op[j].param2 != TEXEL0) && (color->stage[i].op[j].param2 != TEXEL0_ALPHA) &&
						 (color->stage[i].op[j].param2 != TEXEL1) && (color->stage[i].op[j].param2 != TEXEL1_ALPHA)) &&
						 (color->stage[i].op[j].param3 == TEXEL0_ALPHA))
					{
						texEnv->mode = GL_DECAL;
						texEnv->fragment.color = color->stage[i].op[j].param2;
						texEnv->usesT0 = TRUE;
						texEnv->usesT1 = FALSE;
					}
				else if ((color->stage[i].op[j].param1 == TEXEL1) &&
				    ((color->stage[i].op[j].param2 != TEXEL0) && (color->stage[i].op[j].param2 != TEXEL0_ALPHA) &&
					 (color->stage[i].op[j].param2 != TEXEL1) && (color->stage[i].op[j].param2 != TEXEL1_ALPHA)) &&
					 (color->stage[i].op[j].param3 == TEXEL1_ALPHA))
					{
						texEnv->mode = GL_DECAL;
						texEnv->fragment.color = color->stage[i].op[j].param2;
						texEnv->usesT0 = FALSE;
						texEnv->usesT1 = TRUE;
					}
					break;
			}
		}
	}

	return texEnv;
}


void Set_texture_env( TexEnv *texEnv )
{
	combiner.usesT0 = texEnv->usesT0;
	combiner.usesT1 = texEnv->usesT1;
	combiner.usesNoise = FALSE;

	combiner.vertex.color = texEnv->fragment.color;
	combiner.vertex.secondaryColor = COMBINED;
	combiner.vertex.alpha = texEnv->fragment.alpha;

#ifdef PS3
	switch (texEnv->mode)	// texEnv->mode options are: GL_REPLACE, GL_MODULATE, GL_DECAL
	{						// combined_shader modes are: SHADER_PASSTEX=1,SHADER_PASSCOLOR=2,SHADER_MODULATE=3,SHADER_DECAL=4
	case GL_REPLACE:
		OGL.shader_mode = SHADER_PASSTEX;
		break;
	case GL_MODULATE:
		OGL.shader_mode = SHADER_MODULATE;
		break;
	case GL_DECAL:
		OGL.shader_mode = SHADER_DECAL;
		break;
	default:
		OGL.shader_mode = SHADER_PASSCOLOR;
	}

	// TEMPORAL v00357: log combiner-mode transitions to diagnose untextured
	// characters (GoldenEye gray silhouettes). Logs on change + first 60 draws.
#ifdef DEBUG_PROBES
	{
		static int prevMode = -999, prevT0 = -1, prevT1 = -1;
		static int logCount = 0;
		int curMode = (int)OGL.shader_mode;
		if (curMode != prevMode || (int)combiner.usesT0 != prevT0 || (int)combiner.usesT1 != prevT1) {
			prevMode = curMode; prevT0 = (int)combiner.usesT0; prevT1 = (int)combiner.usesT1;
			logCount++;
			if (logCount <= 60) {
				u32 taddr = 0, tW = 0, tH = 0; int tFmt = -1, tSize = -1, tPal = -1; u32 tMem = 0;
				if (cache.current[0]) {
					taddr = cache.current[0]->address; tW = cache.current[0]->realWidth; tH = cache.current[0]->realHeight;
					tFmt = (int)cache.current[0]->format; tSize = (int)cache.current[0]->size;
					tPal = (int)cache.current[0]->palette; tMem = cache.current[0]->tMem;
				}
				DBG_TEX("[TEXMODE] #%d mode=%d usesT0=%d usesT1=%d alphaMode=%.0f texDummy=%d addr=0x%08X fmt=%d size=%d pal=%d tmem=0x%03X %dx%d\n",
					logCount, curMode, (int)combiner.usesT0, (int)combiner.usesT1,
					(double)OGL.shader_alpha_mode,
					(cache.current[0] && cache.current[0]->rsxTextureBuffer) ? 0 : 1,
					taddr, tFmt, tSize, tPal, tMem, tW, tH);
			}
		}
	}
#endif

	// Map alpha combiner output to shader alpha_mode
	// 0 = 1.0 (opaque), 1 = color.a (texel), 2 = color0.a (shade/vertex)
	switch (texEnv->fragment.alpha)
	{
	case SHADE:
	case SHADE_ALPHA:
	case PRIMITIVE_ALPHA:
	case ENV_ALPHA:
	case PRIM_LOD_FRAC:
		OGL.shader_alpha_mode = 2.0f;
		break;
	case TEXEL0_ALPHA:
	case TEXEL1_ALPHA:
		OGL.shader_alpha_mode = texEnv->multiplyByPrimAlpha ? 4.0f : 1.0f;
		break;
	default:
		OGL.shader_alpha_mode = 0.0f;
	}

	rsxLoadFragmentProgramLocation(context,OGL.fpo,OGL.fp_offset,GCM_LOCATION_RSX);
	rsxSetFragmentProgramParameter(context,OGL.fpo,OGL.mode_id,&OGL.shader_mode,OGL.fp_offset);
	rsxSetFragmentProgramParameter(context,OGL.fpo,OGL.alpha_mode_id,&OGL.shader_alpha_mode,OGL.fp_offset);
	if (OGL.primAlpha_id >= 0)
		rsxSetFragmentProgramParameter(context,OGL.fpo,OGL.primAlpha_id,&gDP.primColor.a,OGL.fp_offset);

#elif defined(__GX__)
	u8 GXmode;

	switch (texEnv->mode)	// texEnv->mode options are: GL_REPLACE, GL_MODULATE, GL_DECAL
	{						// TevOps are: GX_MODULATE, GX_DECAL, GX_BLEND, GX_REPLACE, GX_PASSCLR
	case GL_REPLACE:
		GXmode = GX_REPLACE;
		break;
	case GL_MODULATE:
		GXmode = GX_MODULATE;
		break;
	case GL_DECAL:
		GXmode = GX_DECAL;
		break;
	default:
		GXmode = GX_PASSCLR;
	}
	GX_SetNumChans (1);
	GX_SetNumTevStages (1);
	if (texEnv->usesT0)
	{
		GX_SetNumTexGens (1);
		GX_SetTevOrder (GX_TEVSTAGE0, GX_TEXCOORD0, GX_TEXMAP0, GX_COLOR0A0);
	}
	else if (texEnv->usesT1)
	{
		GX_SetNumTexGens (2);
		GX_SetTevOrder (GX_TEVSTAGE0, GX_TEXCOORD1, GX_TEXMAP1, GX_COLOR0A0);
	}
	else 
	{
		GX_SetNumTexGens (0);
		GX_SetTevOrder (GX_TEVSTAGE0, GX_TEXCOORDNULL, GX_TEXMAP_NULL, GX_COLOR0A0);
		GXmode = GX_PASSCLR;
	}
	GX_SetTevOp(GX_TEVSTAGE0,GXmode);
#else // __GX__
	// Shouldn't ever happen, but who knows?
	if (OGL.ARB_multitexture)
		glActiveTextureARB( GL_TEXTURE0_ARB );

	if (texEnv->usesT0 || texEnv->usesT1)
		glEnable( GL_TEXTURE_2D );
	else
		glDisable( GL_TEXTURE_2D );

	glTexEnvi( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, texEnv->mode );
#endif // !__GX__
}
