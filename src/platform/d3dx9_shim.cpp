// d3dx9_shim.cpp - le poche funzioni D3DX usate dal motore, senza D3DX.
// - D3DXCreateBuffer: vera (un blocco di memoria con refcount).
// - Compilazione HLSL a runtime: non disponibile. Serve solo in modalità sviluppo (materiali da sorgente)
//   e per gli shader YUV->RGB dei filmati Bink, che per ora sono disattivati (platform/bink).
// - Screenshot: non ancora implementato.
#if !defined(_WIN32)
#include <platform/compat/d3dx9.h>
#include <atomic>
#include <cstdlib>

namespace
{
struct Buffer final : public ID3DXBuffer
{
    std::atomic<ULONG> refs{ 1 };
    DWORD size;
    void *data;
    explicit Buffer(DWORD n) : size(n), data(calloc(1, n ? n : 1)) {}
    ~Buffer() { free(data); }
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID, void **out) override { if (out) *out = nullptr; return E_NOINTERFACE; }
    ULONG STDMETHODCALLTYPE AddRef() override { return ++refs; }
    ULONG STDMETHODCALLTYPE Release() override { ULONG n = --refs; if (!n) delete this; return n; }
    LPVOID STDMETHODCALLTYPE GetBufferPointer() override { return data; }
    DWORD STDMETHODCALLTYPE GetBufferSize() override { return size; }
};
} // namespace

extern "C" HRESULT D3DXCreateBuffer(DWORD numBytes, LPD3DXBUFFER *buffer)
{
    if (!buffer) return E_INVALIDARG;
    auto *b = new Buffer(numBytes);
    if (!b->data) { delete b; *buffer = nullptr; return E_OUTOFMEMORY; }
    *buffer = b;
    return S_OK;
}

extern "C" HRESULT D3DXCompileShader(LPCSTR, UINT, const D3DXMACRO *, LPD3DXINCLUDE, LPCSTR, LPCSTR, DWORD, LPD3DXBUFFER *shader,
                                     LPD3DXBUFFER *errors, LPD3DXCONSTANTTABLE *constants)
{
    if (shader) *shader = nullptr;
    if (constants) *constants = nullptr;
    if (errors)
    {
        static const char msg[] = "D3DXCompileShader non disponibile su questa piattaforma";
        if (D3DXCreateBuffer(sizeof(msg), errors) == S_OK) memcpy((*errors)->GetBufferPointer(), msg, sizeof(msg));
    }
    return E_NOTIMPL;
}

extern "C" HRESULT D3DXGetShaderConstantTable(const DWORD *, LPD3DXCONSTANTTABLE *table)
{
    if (table) *table = nullptr;
    return E_NOTIMPL;
}

// Le semantiche di ingresso/uscita si leggono dai token DCL del bytecode SM2/SM3: implementazione reale.
static HRESULT ReadSemantics(const DWORD *code, D3DXSEMANTIC *out, UINT *count, bool output)
{
    if (!code || !count) return E_INVALIDARG;
    UINT n = 0;
    const DWORD version = code[0];
    const bool vs = (version & 0xFFFF0000u) == 0xFFFE0000u;
    const DWORD major = (version >> 8) & 0xFF;
    for (const DWORD *p = code + 1; *p != 0x0000FFFFu; )
    {
        DWORD tok = *p;
        DWORD opcode = tok & 0xFFFF;
        if (opcode == 0xFFFE) { p += 1 + ((tok >> 16) & 0x7FFF); continue; } // commento
        DWORD len = (tok >> 24) & 0x0F;
        if (opcode == 0x1F) // dcl
        {
            DWORD usage = p[1] & 0x1F, index = (p[1] >> 16) & 0x0F;
            DWORD reg = p[2];
            DWORD type = ((reg >> 28) & 0x7) | ((reg >> 8) & 0x18);
            bool isOut = vs ? (major >= 3 ? type == 6 /*D3DSPR_OUTPUT*/ : false) : false;
            bool isIn = vs ? type == 1 /*INPUT*/ : (type == 1 || type == 3 /*TEXTURE*/);
            if ((output && isOut) || (!output && isIn))
            {
                if (out) { out[n].Usage = usage; out[n].UsageIndex = index; }
                ++n;
            }
        }
        p += 1 + len;
    }
    *count = n;
    return S_OK;
}

extern "C" HRESULT D3DXGetShaderInputSemantics(const DWORD *f, D3DXSEMANTIC *s, UINT *c) { return ReadSemantics(f, s, c, false); }
extern "C" HRESULT D3DXGetShaderOutputSemantics(const DWORD *f, D3DXSEMANTIC *s, UINT *c) { return ReadSemantics(f, s, c, true); }

extern "C" HRESULT D3DXSaveSurfaceToFileA(LPCSTR, D3DXIMAGE_FILEFORMAT, IDirect3DSurface9 *, const PALETTEENTRY *, const RECT *)
{
    return E_NOTIMPL; // TODO: LockRect + scrittura TGA/JPG
}

extern "C" const char *DXGetErrorStringA(HRESULT hr)
{
    static thread_local char buf[32];
    snprintf(buf, sizeof(buf), "0x%08X", static_cast<unsigned>(hr));
    return buf;
}
#endif
