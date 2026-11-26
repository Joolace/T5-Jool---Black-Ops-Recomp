#include "r_hw_nvidia.h"
#include "r_init.h"

bool __cdecl NV_UseShadowNullColorRenderTarget()
{
#ifdef NX_DXVK
    // DXVK binds no colour attachment for a NULL render target, so shadow
    // maps render depth only.
    return true;
#else
    return dx.nvInitialized;
#endif
}

