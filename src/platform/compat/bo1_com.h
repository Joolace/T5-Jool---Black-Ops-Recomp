// bo1_com.h - il minimo di COM che serve a XAudio2/XAPO fuori da Windows.
// Se DXVK-native è presente usiamo il suo <unknwn.h> (stesso IUnknown del d3d9.h di DXVK).
#pragma once

#include "win_compat.h"

#if __has_include(<unknwn.h>)
#include <unknwn.h>
// GUID associato a un tipo: con DXVK passa dal suo __uuidof_helper
#define BO1_DEFINE_UUID(type, d1, d2, d3, b0, b1, b2, b3, b4, b5, b6, b7) \
    DECLARE_UUIDOF_HELPER(type, d1, d2, d3, b0, b1, b2, b3, b4, b5, b6, b7)
#ifndef STDMETHODIMP
#define STDMETHODIMP HRESULT STDMETHODCALLTYPE
#define STDMETHODIMP_(type) type STDMETHODCALLTYPE
#endif
#else
#ifdef __cplusplus
typedef const GUID &REFGUID;
typedef const IID &REFIID;
static inline bool operator==(const GUID &a, const GUID &b) { return memcmp(&a, &b, sizeof(GUID)) == 0; }
static inline bool operator!=(const GUID &a, const GUID &b) { return !(a == b); }
#else
typedef const GUID *REFGUID;
typedef const IID *REFIID;
#endif

#ifndef STDMETHODCALLTYPE
#define STDMETHODCALLTYPE __stdcall
#endif
#define STDMETHOD(name) virtual HRESULT STDMETHODCALLTYPE name
#define STDMETHOD_(type, name) virtual type STDMETHODCALLTYPE name
#define STDMETHODIMP HRESULT STDMETHODCALLTYPE
#define STDMETHODIMP_(type) type STDMETHODCALLTYPE
#define PURE = 0
#define THIS_
#define THIS void
#define DECLARE_INTERFACE(iface) struct iface
#define DECLARE_INTERFACE_(iface, base) struct iface : public base
#ifndef interface
#define interface struct
#endif
#define E_POINTER ((HRESULT)0x80004003)
#define E_NOINTERFACE ((HRESULT)0x80004002)
#define E_NOTIMPL ((HRESULT)0x80004001)

// GUID associato a un tipo, al posto di __declspec(uuid)/__uuidof di MSVC.
template <typename T> struct bo1_uuid_of;
#define BO1_DEFINE_UUID(type, d1, d2, d3, b0, b1, b2, b3, b4, b5, b6, b7) \
    template <> struct bo1_uuid_of<type> { static const GUID &get() { static const GUID g = { d1, d2, d3, { b0, b1, b2, b3, b4, b5, b6, b7 } }; return g; } }

struct IUnknown
{
    virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppvObject) = 0;
    virtual ULONG STDMETHODCALLTYPE AddRef() = 0;
    virtual ULONG STDMETHODCALLTYPE Release() = 0;
};
BO1_DEFINE_UUID(IUnknown, 0x00000000, 0x0000, 0x0000, 0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46);

#ifndef __uuidof
#define __uuidof(T) (bo1_uuid_of<T>::get())
#endif
#endif // unknwn.h
