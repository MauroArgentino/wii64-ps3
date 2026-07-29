#ifdef PS3
#include "../../main/winlnxdefs.h"
#undef NULL
#define NULL 0
#elif defined(__GX__)
#include <gccore.h>
#endif

#include <malloc.h>
#include "DepthBuffer.h"
#include "Types.h"
#include "gSP.h"
#include "gDP.h"

DepthBufferInfo depthBuffer;

static void _buildZlut()
{
	for (u32 i = 0; i < 0x40000; i++) {
		u32 exponent = 0;
		u32 testbit = 1 << 17;
		while ((i & testbit) != 0u && exponent < 7) {
			exponent++;
			testbit = 1 << (17 - exponent);
		}
		u32 mantissa = (i >> (6 - (6 < exponent ? 6 : exponent))) & 0x7ff;
		depthBuffer.zLUT[i] = (u16)(((exponent << 11) | mantissa) << 2);
	}
}

void DepthBuffer_Init()
{
	depthBuffer.current = NULL;
	depthBuffer.top = NULL;
	depthBuffer.bottom = NULL;
	depthBuffer.numBuffers = 0;
	depthBuffer.zLUT = (u16*)malloc(0x40000 * sizeof(u16));
	if (depthBuffer.zLUT != NULL)
		_buildZlut();
}

void DepthBuffer_RemoveBottom()
{
	DepthBuffer *newBottom = depthBuffer.bottom->higher;

	if (depthBuffer.bottom == depthBuffer.top)
		depthBuffer.top = NULL;

	free(depthBuffer.bottom);

	depthBuffer.bottom = newBottom;

	if (depthBuffer.bottom != NULL)
		depthBuffer.bottom->lower = NULL;

	depthBuffer.numBuffers--;
}

void DepthBuffer_Remove(DepthBuffer *buffer)
{
	if (buffer == depthBuffer.bottom && buffer == depthBuffer.top)
	{
		depthBuffer.top = NULL;
		depthBuffer.bottom = NULL;
	}
	else if (buffer == depthBuffer.bottom)
	{
		depthBuffer.bottom = buffer->higher;

		if (depthBuffer.bottom)
			depthBuffer.bottom->lower = NULL;
	}
	else if (buffer == depthBuffer.top)
	{
		depthBuffer.top = buffer->lower;

		if (depthBuffer.top)
			depthBuffer.top->higher = NULL;
	}
	else
	{
		buffer->higher->lower = buffer->lower;
		buffer->lower->higher = buffer->higher;
	}

	free(buffer);

	depthBuffer.numBuffers--;
}

void DepthBuffer_RemoveBuffer(u32 address)
{
	DepthBuffer *current = depthBuffer.bottom;

	while (current != NULL)
	{
		if (current->address == address)
		{
			DepthBuffer_Remove(current);
			return;
		}
		current = current->higher;
	}
}

DepthBuffer *DepthBuffer_AddTop()
{
	DepthBuffer *newtop = (DepthBuffer*)malloc(sizeof(DepthBuffer));

	newtop->lower = depthBuffer.top;
	newtop->higher = NULL;
	newtop->width = 0;

	if (depthBuffer.top)
		depthBuffer.top->higher = newtop;

	if (!depthBuffer.bottom)
		depthBuffer.bottom = newtop;

	depthBuffer.top = newtop;

	depthBuffer.numBuffers++;

	return newtop;
}

void DepthBuffer_MoveToTop(DepthBuffer *newtop)
{
	if (newtop == depthBuffer.top)
		return;

	if (newtop == depthBuffer.bottom)
	{
		depthBuffer.bottom = newtop->higher;
		depthBuffer.bottom->lower = NULL;
	}
	else
	{
		newtop->higher->lower = newtop->lower;
		newtop->lower->higher = newtop->higher;
	}

	newtop->higher = NULL;
	newtop->lower = depthBuffer.top;
	depthBuffer.top->higher = newtop;
	depthBuffer.top = newtop;
}

void DepthBuffer_Destroy()
{
	while (depthBuffer.bottom)
		DepthBuffer_RemoveBottom();
	depthBuffer.top = NULL;

	if (depthBuffer.zLUT != NULL) {
		free(depthBuffer.zLUT);
		depthBuffer.zLUT = NULL;
	}
}

void DepthBuffer_SetBuffer(u32 address)
{
	DepthBuffer *current = depthBuffer.top;

	while (current != NULL)
	{
		if (current->address == address)
		{
			DepthBuffer_MoveToTop(current);
			depthBuffer.current = current;
			return;
		}
		current = current->lower;
	}

	current = DepthBuffer_AddTop();

	current->address = address;
	current->cleared = TRUE;
	current->width = gDP.colorImage.width;

	depthBuffer.current = current;
}

DepthBuffer *DepthBuffer_FindBuffer(u32 address)
{
	DepthBuffer *current = depthBuffer.top;

	while (current)
	{
		if (current->address == address)
			return current;
		current = current->lower;
	}

	return NULL;
}

void DepthBuffer_Clear()
{
	if (depthBuffer.current != NULL)
		depthBuffer.current->cleared = TRUE;

	for (DepthBuffer *buf = depthBuffer.bottom; buf != NULL; buf = buf->higher)
		buf->cleared = TRUE;
}
