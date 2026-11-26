// XAPOBase.h - CXAPOBase (classe base degli effetti XAPO dell'SDK), riscritta per Linux/Android.
// Implementazione in xapobase.cpp: stesso comportamento della versione dell'SDK di giugno 2010.
#pragma once

#include "XAPO.h"

class CXAPOBase : public IXAPO
{
private:
    const XAPO_REGISTRATION_PROPERTIES *m_pRegistrationProperties;
    void *m_pfnMatrixMixFunction;
    float *m_pfl32MatrixCoefficients;
    UINT32 m_nSrcFormatType;
    BOOL m_fIsScalarMatrix;
    BOOL m_fIsLocked;

protected:
    LONG m_lReferenceCount;

    virtual HRESULT ValidateFormatDefault(WAVEFORMATEX *pFormat, BOOL fOverwrite);
    virtual HRESULT ValidateFormatPair(const WAVEFORMATEX *pSupportedFormat, WAVEFORMATEX *pRequestedFormat, BOOL fOverwrite);
    void ProcessThru(void *pInputBuffer, float *pOutputBuffer, UINT32 FrameCount, WORD InputChannelCount, WORD OutputChannelCount, BOOL MixWithOutput);
    const XAPO_REGISTRATION_PROPERTIES *GetRegistrationPropertiesInternal() { return m_pRegistrationProperties; }
    BOOL IsLocked() { return m_fIsLocked; }

public:
    CXAPOBase(const XAPO_REGISTRATION_PROPERTIES *pRegistrationProperties);
    virtual ~CXAPOBase();

    STDMETHOD(QueryInterface)(REFIID riid, void **ppInterface) override;
    STDMETHOD_(ULONG, AddRef)() override;
    STDMETHOD_(ULONG, Release)() override;

    STDMETHOD(GetRegistrationProperties)(XAPO_REGISTRATION_PROPERTIES **ppRegistrationProperties) override;
    STDMETHOD(IsInputFormatSupported)(const WAVEFORMATEX *pOutputFormat, const WAVEFORMATEX *pRequestedInputFormat, WAVEFORMATEX **ppSupportedInputFormat) override;
    STDMETHOD(IsOutputFormatSupported)(const WAVEFORMATEX *pInputFormat, const WAVEFORMATEX *pRequestedOutputFormat, WAVEFORMATEX **ppSupportedOutputFormat) override;
    STDMETHOD(Initialize)(const void *, UINT32 DataByteSize) override;
    STDMETHOD_(void, Reset)() override {}
    STDMETHOD(LockForProcess)(UINT32 InputLockedParameterCount, const XAPO_LOCKFORPROCESS_BUFFER_PARAMETERS *pInputLockedParameters,
        UINT32 OutputLockedParameterCount, const XAPO_LOCKFORPROCESS_BUFFER_PARAMETERS *pOutputLockedParameters) override;
    STDMETHOD_(void, UnlockForProcess)() override;
    STDMETHOD_(UINT32, CalcInputFrames)(UINT32 OutputFrameCount) override { return OutputFrameCount; }
    STDMETHOD_(UINT32, CalcOutputFrames)(UINT32 InputFrameCount) override { return InputFrameCount; }
};
