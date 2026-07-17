/**
 * NV40 Alpha Test register writes for N64 emulation.
 * Extracted from rsxutil.cpp to avoid symbol conflicts with Tiny3D.
 * These write directly to NV40TCL registers that PSL1GHT's rsx* API
 * does not expose.
 */

#include <rsx/rsx.h>
#include <rsx/nv40.h>
#include <ppu-types.h>

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
