#pragma once

#include <windows.h>
#include <oleidl.h>
#include <string>
#include <vector>

namespace EasyZip
{
    class OleDropSource : public IDropSource
    {
    private:
        LONG m_refCount;

    public:
        OleDropSource();
        virtual ~OleDropSource() = default;

        // IUnknown
        STDMETHODIMP QueryInterface(REFIID riid, void** ppv) override;
        STDMETHODIMP_(ULONG) AddRef() override;
        STDMETHODIMP_(ULONG) Release() override;

        // IDropSource
        STDMETHODIMP QueryContinueDrag(BOOL fEscapePressed, DWORD grfKeyState) override;
        STDMETHODIMP GiveFeedback(DWORD dwEffect) override;
    };

    class OleDataObject : public IDataObject
    {
    private:
        struct DataEntry
        {
            FORMATETC formatEtc;
            STGMEDIUM stgMedium;
        };

        LONG m_refCount;
        std::vector<DataEntry> m_entries;

    public:
        OleDataObject();
        virtual ~OleDataObject();

        // Factory helper to create IDataObject from list of absolute file paths (CF_HDROP + CFSTR_PREFERREDDROPEFFECT)
        static HRESULT Create(const std::vector<std::wstring>& files, IDataObject** ppDataObject);

        // IUnknown
        STDMETHODIMP QueryInterface(REFIID riid, void** ppv) override;
        STDMETHODIMP_(ULONG) AddRef() override;
        STDMETHODIMP_(ULONG) Release() override;

        // IDataObject
        STDMETHODIMP GetData(FORMATETC* pformatetcIn, STGMEDIUM* pmedium) override;
        STDMETHODIMP GetDataHere(FORMATETC* pformatetc, STGMEDIUM* pmedium) override;
        STDMETHODIMP QueryGetData(FORMATETC* pformatetc) override;
        STDMETHODIMP GetCanonicalFormatEtc(FORMATETC* pformatectIn, FORMATETC* pformatetcOut) override;
        STDMETHODIMP SetData(FORMATETC* pformatetc, STGMEDIUM* pmedium, BOOL fRelease) override;
        STDMETHODIMP EnumFormatEtc(DWORD dwDirection, IEnumFORMATETC** ppenumFormatEtc) override;
        STDMETHODIMP DAdvise(FORMATETC* pformatetc, DWORD advf, IAdviseSink* pAdvSink, DWORD* pdwConnection) override;
        STDMETHODIMP DUnadvise(DWORD dwConnection) override;
        STDMETHODIMP EnumDAdvise(IEnumSTATDATA** ppenumAdvise) override;
    };
}
