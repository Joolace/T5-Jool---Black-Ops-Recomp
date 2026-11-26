#pragma once
// d3dx9.h (Linux/Android): D3DX non esiste fuori da Windows (DXVK fornisce solo d3d9).
// Il motore usa pochissimo D3DX: buffer, compilazione shader HLSL a runtime (solo in modalità sviluppo e
// per i filmati Bink) e salvataggio screenshot. Implementazione in platform/d3dx9_shim.cpp.
#include <d3d9.h>
#include "bo1_com.h"

typedef enum _D3DXIMAGE_FILEFORMAT { D3DXIFF_BMP = 0, D3DXIFF_JPG = 1, D3DXIFF_TGA = 2, D3DXIFF_PNG = 3, D3DXIFF_DDS = 4 } D3DXIMAGE_FILEFORMAT;
#ifndef BO1_HAVE_D3DXSEMANTIC
#define BO1_HAVE_D3DXSEMANTIC
typedef struct _D3DXSEMANTIC { UINT Usage; UINT UsageIndex; } D3DXSEMANTIC, *LPD3DXSEMANTIC;
#endif
typedef struct _D3DXMACRO { LPCSTR Name; LPCSTR Definition; } D3DXMACRO, *LPD3DXMACRO;
typedef const char *D3DXHANDLE;

struct ID3DXBuffer : public IUnknown
{
    virtual LPVOID STDMETHODCALLTYPE GetBufferPointer() = 0;
    virtual DWORD STDMETHODCALLTYPE GetBufferSize() = 0;
};
typedef ID3DXBuffer *LPD3DXBUFFER;

struct ID3DXInclude;
typedef ID3DXInclude *LPD3DXINCLUDE;

// Tabella delle costanti dentro il bytecode degli shader (formato del file, solo DWORD/WORD: uguale a 32 e 64 bit)
typedef struct _D3DXSHADER_CONSTANTTABLE {
    DWORD Size, Creator, Version, Constants, ConstantInfo, Flags, Target;
} D3DXSHADER_CONSTANTTABLE, *LPD3DXSHADER_CONSTANTTABLE;
typedef struct _D3DXSHADER_CONSTANTINFO {
    DWORD Name; WORD RegisterSet, RegisterIndex, RegisterCount, Reserved; DWORD TypeInfo, DefaultValue;
} D3DXSHADER_CONSTANTINFO, *LPD3DXSHADER_CONSTANTINFO;
typedef struct _D3DXSHADER_TYPEINFO {
    WORD Class, Type, Rows, Columns, Elements, StructMembers; DWORD StructMemberInfo;
} D3DXSHADER_TYPEINFO, *LPD3DXSHADER_TYPEINFO;

typedef struct _D3DXCONSTANTTABLE_DESC { LPCSTR Creator; DWORD Version; UINT Constants; } D3DXCONSTANTTABLE_DESC;
typedef enum _D3DXREGISTER_SET { D3DXRS_BOOL, D3DXRS_INT4, D3DXRS_FLOAT4, D3DXRS_SAMPLER } D3DXREGISTER_SET;
typedef struct _D3DXCONSTANT_DESC {
    LPCSTR Name; D3DXREGISTER_SET RegisterSet; UINT RegisterIndex, RegisterCount; int Class, Type; UINT Rows, Columns, Elements, StructMembers, Bytes; LPCVOID DefaultValue;
} D3DXCONSTANT_DESC;
struct ID3DXConstantTable : public ID3DXBuffer
{
    virtual HRESULT STDMETHODCALLTYPE GetDesc(D3DXCONSTANTTABLE_DESC *desc) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetConstantDesc(D3DXHANDLE c, D3DXCONSTANT_DESC *desc, UINT *count) = 0;
    virtual D3DXHANDLE STDMETHODCALLTYPE GetConstant(D3DXHANDLE parent, UINT index) = 0;
};
typedef ID3DXConstantTable *LPD3DXCONSTANTTABLE;

#define D3DXSHADER_DEBUG 0x1
#define D3DXSHADER_SKIPOPTIMIZATION 0x4
#define D3DXSHADER_PACKMATRIX_ROWMAJOR 0x8
#define D3DXSHADER_OPTIMIZATION_LEVEL3 0x8000
#define D3DXERR_INVALIDDATA ((HRESULT)0x88760b59)

#ifdef __cplusplus
extern "C" {
#endif
HRESULT D3DXCreateBuffer(DWORD numBytes, LPD3DXBUFFER *buffer);
HRESULT D3DXCompileShader(LPCSTR src, UINT srcLen, const D3DXMACRO *defines, LPD3DXINCLUDE include, LPCSTR entry, LPCSTR profile,
                          DWORD flags, LPD3DXBUFFER *shader, LPD3DXBUFFER *errors, LPD3DXCONSTANTTABLE *constants);
HRESULT D3DXGetShaderConstantTable(const DWORD *function, LPD3DXCONSTANTTABLE *table);
HRESULT D3DXGetShaderInputSemantics(const DWORD *function, D3DXSEMANTIC *semantics, UINT *count);
HRESULT D3DXGetShaderOutputSemantics(const DWORD *function, D3DXSEMANTIC *semantics, UINT *count);
HRESULT D3DXSaveSurfaceToFileA(LPCSTR file, D3DXIMAGE_FILEFORMAT fmt, IDirect3DSurface9 *surface, const PALETTEENTRY *palette, const RECT *rect);
#define D3DXSaveSurfaceToFile D3DXSaveSurfaceToFileA
#ifdef __cplusplus
}
#endif
