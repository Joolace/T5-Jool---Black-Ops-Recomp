#pragma once

// SaltyNX is an Atmosphere sysmodule; Switch 2 (kaiHEN) has none.
#ifdef KISAK_NX2
inline void NX_SaltyNXStart() {}
inline void NX_SaltyNXStop() {}
inline void NX_SaltyNXFrame(unsigned, unsigned) {}
#else
void NX_SaltyNXStart();
void NX_SaltyNXStop();
void NX_SaltyNXFrame(unsigned width, unsigned height);
#endif
