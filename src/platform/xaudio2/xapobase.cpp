// xapobase.cpp - CXAPOBase (classe base degli effetti audio dell'SDK di giugno 2010), riscritta.
// Gli effetti del motore (SDXA2Effect) derivano da qui e sovrascrivono Process/LockForProcess.
#if !defined(_WIN32)

#include "XAPOBase.h"

CXAPOBase::CXAPOBase(const XAPO_REGISTRATION_PROPERTIES *props)
    : m_pRegistrationProperties(props), m_pfnMatrixMixFunction(nullptr), m_pfl32MatrixCoefficients(nullptr),
      m_nSrcFormatType(0), m_fIsScalarMatrix(FALSE), m_fIsLocked(FALSE), m_lReferenceCount(1)
{
}

CXAPOBase::~CXAPOBase() {}

HRESULT CXAPOBase::QueryInterface(REFIID riid, void **ppInterface)
{
    if (!ppInterface) return E_POINTER;
    if (riid == __uuidof(IXAPO) || riid == __uuidof(IUnknown))
    {
        *ppInterface = static_cast<IXAPO *>(this);
        AddRef();
        return S_OK;
    }
    *ppInterface = nullptr;
    return E_NOINTERFACE;
}

ULONG CXAPOBase::AddRef() { return static_cast<ULONG>(InterlockedIncrement(&m_lReferenceCount)); }

ULONG CXAPOBase::Release()
{
    LONG n = InterlockedDecrement(&m_lReferenceCount);
    if (n == 0) delete this;
    return static_cast<ULONG>(n);
}

HRESULT CXAPOBase::GetRegistrationProperties(XAPO_REGISTRATION_PROPERTIES **out)
{
    if (!out) return E_POINTER;
    auto *copy = static_cast<XAPO_REGISTRATION_PROPERTIES *>(malloc(sizeof(XAPO_REGISTRATION_PROPERTIES)));
    if (!copy) return E_OUTOFMEMORY;
    *copy = *m_pRegistrationProperties;
    *out = copy; // l'host lo libera con free (su Windows: XAPOFree)
    return S_OK;
}

// Formato di default: float 32 bit, interleaved, con un numero di canali e frequenza validi.
HRESULT CXAPOBase::ValidateFormatDefault(WAVEFORMATEX *fmt, BOOL overwrite)
{
    bool ok = fmt->wFormatTag == WAVE_FORMAT_IEEE_FLOAT || fmt->wFormatTag == WAVE_FORMAT_EXTENSIBLE;
    ok = ok && fmt->wBitsPerSample == 32 && fmt->nChannels >= XAPO_MIN_CHANNELS && fmt->nChannels <= XAPO_MAX_CHANNELS;
    ok = ok && fmt->nSamplesPerSec >= XAPO_MIN_FRAMERATE && fmt->nSamplesPerSec <= XAPO_MAX_FRAMERATE;
    if (ok) return S_OK;
    if (overwrite)
    {
        fmt->wFormatTag = WAVE_FORMAT_IEEE_FLOAT;
        fmt->wBitsPerSample = 32;
        if (fmt->nChannels < XAPO_MIN_CHANNELS) fmt->nChannels = XAPO_MIN_CHANNELS;
        if (fmt->nChannels > XAPO_MAX_CHANNELS) fmt->nChannels = XAPO_MAX_CHANNELS;
        if (fmt->nSamplesPerSec < XAPO_MIN_FRAMERATE) fmt->nSamplesPerSec = XAPO_MIN_FRAMERATE;
        if (fmt->nSamplesPerSec > XAPO_MAX_FRAMERATE) fmt->nSamplesPerSec = XAPO_MAX_FRAMERATE;
        fmt->nBlockAlign = static_cast<WORD>(fmt->nChannels * 4);
        fmt->nAvgBytesPerSec = fmt->nSamplesPerSec * fmt->nBlockAlign;
    }
    return S_FALSE;
}

HRESULT CXAPOBase::ValidateFormatPair(const WAVEFORMATEX *supported, WAVEFORMATEX *requested, BOOL overwrite)
{
    HRESULT hr = ValidateFormatDefault(requested, overwrite);
    UINT32 flags = m_pRegistrationProperties->Flags;
    bool match = true;
    if ((flags & XAPO_FLAG_CHANNELS_MUST_MATCH) && requested->nChannels != supported->nChannels) { match = false; if (overwrite) requested->nChannels = supported->nChannels; }
    if ((flags & XAPO_FLAG_FRAMERATE_MUST_MATCH) && requested->nSamplesPerSec != supported->nSamplesPerSec) { match = false; if (overwrite) requested->nSamplesPerSec = supported->nSamplesPerSec; }
    if ((flags & XAPO_FLAG_BITSPERSAMPLE_MUST_MATCH) && requested->wBitsPerSample != supported->wBitsPerSample) { match = false; if (overwrite) requested->wBitsPerSample = supported->wBitsPerSample; }
    if (!match && overwrite)
    {
        requested->nBlockAlign = static_cast<WORD>(requested->nChannels * requested->wBitsPerSample / 8);
        requested->nAvgBytesPerSec = requested->nSamplesPerSec * requested->nBlockAlign;
    }
    return match ? hr : S_FALSE;
}

HRESULT CXAPOBase::IsInputFormatSupported(const WAVEFORMATEX *outFmt, const WAVEFORMATEX *reqIn, WAVEFORMATEX **supportedIn)
{
    WAVEFORMATEX tmp = *reqIn;
    HRESULT hr = ValidateFormatPair(outFmt, &tmp, supportedIn != nullptr);
    if (hr != S_OK && supportedIn)
    {
        *supportedIn = static_cast<WAVEFORMATEX *>(malloc(sizeof(WAVEFORMATEX)));
        **supportedIn = tmp;
    }
    return hr;
}

HRESULT CXAPOBase::IsOutputFormatSupported(const WAVEFORMATEX *inFmt, const WAVEFORMATEX *reqOut, WAVEFORMATEX **supportedOut)
{
    WAVEFORMATEX tmp = *reqOut;
    HRESULT hr = ValidateFormatPair(inFmt, &tmp, supportedOut != nullptr);
    if (hr != S_OK && supportedOut)
    {
        *supportedOut = static_cast<WAVEFORMATEX *>(malloc(sizeof(WAVEFORMATEX)));
        **supportedOut = tmp;
    }
    return hr;
}

HRESULT CXAPOBase::Initialize(const void *, UINT32) { return S_OK; }

HRESULT CXAPOBase::LockForProcess(UINT32, const XAPO_LOCKFORPROCESS_BUFFER_PARAMETERS *, UINT32, const XAPO_LOCKFORPROCESS_BUFFER_PARAMETERS *)
{
    if (m_fIsLocked) return XAUDIO2_E_INVALID_CALL;
    m_fIsLocked = TRUE;
    return S_OK;
}

void CXAPOBase::UnlockForProcess() { m_fIsLocked = FALSE; }

// Copia (o somma) l'ingresso nell'uscita convertendo il numero di canali: mono<->stereo e uguale->uguale.
void CXAPOBase::ProcessThru(void *in, float *out, UINT32 frames, WORD inCh, WORD outCh, BOOL mix)
{
    const float *src = static_cast<const float *>(in);
    for (UINT32 f = 0; f < frames; ++f)
    {
        for (WORD c = 0; c < outCh; ++c)
        {
            float v;
            if (inCh == outCh) v = src[f * inCh + c];
            else if (inCh == 1) v = src[f];
            else if (outCh == 1) { v = 0; for (WORD k = 0; k < inCh; ++k) v += src[f * inCh + k]; v /= inCh; }
            else v = c < inCh ? src[f * inCh + c] : 0.0f;
            if (mix) out[f * outCh + c] += v;
            else out[f * outCh + c] = v;
        }
    }
}

#endif // !_WIN32
