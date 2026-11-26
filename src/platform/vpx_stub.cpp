// vpx_stub.cpp - registrazione video (avi/vpx) disattivata quando la build non trova libvpx.
#include <vpx/vpx.h>
void __cdecl vpx_init(const char *, int, int) {}
void __cdecl write_ivf_file_header(FILE *, const vpx_codec_enc_cfg *, unsigned int) {}
void __cdecl mem_put_le16(char *mem, __int16 val) { mem[0] = (char)val; mem[1] = (char)(val >> 8); }
void __cdecl mem_put_le32(char *mem, unsigned int val) { for (int i = 0; i < 4; ++i) mem[i] = (char)(val >> (8 * i)); }
void __cdecl vpx_encode_frame(unsigned __int8 *, unsigned __int8 *, unsigned __int8 *, bool) {}
void __cdecl write_ivf_frame_header(FILE *, const vpx_codec_cx_pkt *) {}
void __cdecl vpx_shutdown() {}
