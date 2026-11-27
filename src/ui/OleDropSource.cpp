#include "OleDropSource.h"
#include <shlobj.h>

namespace EasyZip
{
    // ========================================================
    // OleDropSource Implementation
    // ========================================================

    OleDropSource::OleDropSource() : m_refCount(1) {}

    STDMETHODIMP OleDropSource::QueryInterface(REFIID riid, void** ppv)
    {
        if (!ppv) return E_POINTER;
        *ppv = nullptr;

        if (IsEqualIID(riid, IID_IUnknown) || IsEqualIID(riid, IID_IDropSource))
        {
            *ppv = static_cast<IDropSource*>(this);
            AddRef();
            return S_OK;
        }
        return E_NOINTERFACE;
    }

    STDMETHODIMP_(ULONG) OleDropSource::AddRef()
    {
        return InterlockedIncrement(&m_refCount);
    }

    STDMETHODIMP_(ULONG) OleDropSource::Release()
    {
        LONG count = InterlockedDecrement(&m_refCount);
        if (count == 0)
        {
            delete this;
        }
        return count;
    }

    STDMETHODIMP OleDropSource::QueryContinueDrag(BOOL fEscapePressed, DWORD grfKeyState)
    {
        if (fEscapePressed)
        {
            return DRAGDROP_S_CANCEL;
        }

        // Drop when left mouse button is released
        if (!(grfKeyState & MK_LBUTTON))
        {
            return DRAGDROP_S_DROP;
        }

        return S_OK;
    }

    STDMETHODIMP OleDropSource::GiveFeedback(DWORD dwEffect)
    {
        return DRAGDROP_S_USEDEFAULTCURSORS;
    }

    // ========================================================
    // Simple EnumFORMATETC Implementation
    // ========================================================

    class EnumFormatEtcImpl : public IEnumFORMATETC
    {
    private:
        LONG m_refCount;
        std::vector<FORMATETC> m_formats;
        size_t m_index;

    public:
        EnumFormatEtcImpl(const std::vector<FORMATETC>& formats)
            : m_refCount(1), m_formats(formats), m_index(0) {}

        STDMETHODIMP QueryInterface(REFIID riid, void** ppv) override
        {
            if (!ppv) return E_POINTER;
            *ppv = nullptr;

            if (IsEqualIID(riid, IID_IUnknown) || IsEqualIID(riid, IID_IEnumFORMATETC))
            {
                *ppv = static_cast<IEnumFORMATETC*>(this);
                AddRef();
                return S_OK;
            }
            return E_NOINTERFACE;
        }

        STDMETHODIMP_(ULONG) AddRef() override { return InterlockedIncrement(&m_refCount); }
        STDMETHODIMP_(ULONG) Release() override
        {
            LONG count = InterlockedDecrement(&m_refCount);
            if (count == 0) delete this;
            return count;
        }

        STDMETHODIMP Next(ULONG celt, FORMATETC* rgelt, ULONG* pceltFetched) override
        {
            if (!rgelt) return E_POINTER;
            ULONG fetched = 0;
            while (m_index < m_formats.size() && fetched < celt)
            {
                rgelt[fetched++] = m_formats[m_index++];
            }
            if (pceltFetched) *pceltFetched = fetched;
            return (fetched == celt) ? S_OK : S_FALSE;
        }

        STDMETHODIMP Skip(ULONG celt) override
        {
            m_index += celt;
            if (m_index > m_formats.size())
            {
                m_index = m_formats.size();
                return S_FALSE;
            }
            return S_OK;
        }

        STDMETHODIMP Reset() override
        {
            m_index = 0;
            return S_OK;
        }

        STDMETHODIMP Clone(IEnumFORMATETC** ppenum) override
        {
            if (!ppenum) return E_POINTER;
            EnumFormatEtcImpl* pNew = new EnumFormatEtcImpl(m_formats);
            pNew->m_index = m_index;
            *ppenum = pNew;
            return S_OK;
        }
    };

    // ========================================================
    // OleDataObject Implementation
    // ========================================================

    OleDataObject::OleDataObject() : m_refCount(1)
    {
    }

    OleDataObject::~OleDataObject()
    {
        for (auto& entry : m_entries)
        {
            if (entry.stgMedium.hGlobal)
            {
                ReleaseStgMedium(&entry.stgMedium);
            }
        }
    }

    HRESULT OleDataObject::Create(const std::vector<std::wstring>& files, IDataObject** ppDataObject)
    {
        if (!ppDataObject) return E_POINTER;
        *ppDataObject = nullptr;
        if (files.empty()) return E_INVALIDARG;

        // Calculate size needed for DROPFILES structure and double-null terminated string
        size_t totalChars = 0;
        for (const auto& f : files)
        {
            totalChars += f.length() + 1;
        }
        totalChars += 1; // Final null terminator

        size_t totalBytes = sizeof(DROPFILES) + (totalChars * sizeof(wchar_t));
        HGLOBAL hDrop = GlobalAlloc(GHND | GMEM_SHARE, totalBytes);
        if (!hDrop) return E_OUTOFMEMORY;

        BYTE* pData = static_cast<BYTE*>(GlobalLock(hDrop));
        if (!pData)
        {
            GlobalFree(hDrop);
            return E_OUTOFMEMORY;
        }

        DROPFILES* pdf = reinterpret_cast<DROPFILES*>(pData);
        pdf->pFiles = sizeof(DROPFILES);
        pdf->pt.x = 0;
        pdf->pt.y = 0;
        pdf->fNC = FALSE;
        pdf->fWide = TRUE;

        wchar_t* pFilenames = reinterpret_cast<wchar_t*>(pData + sizeof(DROPFILES));
        for (const auto& f : files)
        {
            wcscpy_s(pFilenames, f.length() + 1, f.c_str());
            pFilenames += f.length() + 1;
        }
        *pFilenames = L'\0'; // Double null terminator

        GlobalUnlock(hDrop);

        OleDataObject* pObj = new (std::nothrow) OleDataObject();
        if (!pObj)
        {
            GlobalFree(hDrop);
            return E_OUTOFMEMORY;
        }

        // 1. Register CF_HDROP
        FORMATETC fmtDrop = { CF_HDROP, nullptr, DVASPECT_CONTENT, -1, TYMED_HGLOBAL };
        STGMEDIUM stgDrop = { 0 };
        stgDrop.tymed = TYMED_HGLOBAL;
        stgDrop.hGlobal = hDrop;
        pObj->m_entries.push_back({ fmtDrop, stgDrop });

        // 2. Register CFSTR_PREFERREDDROPEFFECT (DROPEFFECT_COPY)
        // This is critical for Windows Explorer to accept .exe and system files safely as copy
        UINT cfPref = RegisterClipboardFormatW(CFSTR_PREFERREDDROPEFFECT);
        if (cfPref)
        {
            HGLOBAL hPref = GlobalAlloc(GHND | GMEM_SHARE, sizeof(DWORD));
            if (hPref)
            {
                DWORD* pEffect = static_cast<DWORD*>(GlobalLock(hPref));
                if (pEffect)
                {
                    *pEffect = DROPEFFECT_COPY;
                    GlobalUnlock(hPref);

                    FORMATETC fmtPref = { static_cast<CLIPFORMAT>(cfPref), nullptr, DVASPECT_CONTENT, -1, TYMED_HGLOBAL };
                    STGMEDIUM stgPref = { 0 };
                    stgPref.tymed = TYMED_HGLOBAL;
                    stgPref.hGlobal = hPref;
                    pObj->m_entries.push_back({ fmtPref, stgPref });
                }
                else
                {
                    GlobalFree(hPref);
                }
            }
        }

        *ppDataObject = pObj;
        return S_OK;
    }

    STDMETHODIMP OleDataObject::QueryInterface(REFIID riid, void** ppv)
    {
        if (!ppv) return E_POINTER;
        *ppv = nullptr;

        if (IsEqualIID(riid, IID_IUnknown) || IsEqualIID(riid, IID_IDataObject))
        {
            *ppv = static_cast<IDataObject*>(this);
            AddRef();
            return S_OK;
        }
        return E_NOINTERFACE;
    }

    STDMETHODIMP_(ULONG) OleDataObject::AddRef()
    {
        return InterlockedIncrement(&m_refCount);
    }

    STDMETHODIMP_(ULONG) OleDataObject::Release()
    {
        LONG count = InterlockedDecrement(&m_refCount);
        if (count == 0)
        {
            delete this;
        }
        return count;
    }

    STDMETHODIMP OleDataObject::GetData(FORMATETC* pformatetcIn, STGMEDIUM* pmedium)
    {
        if (!pformatetcIn || !pmedium) return E_POINTER;
        pmedium->tymed = TYMED_NULL;
        pmedium->hGlobal = nullptr;
        pmedium->pUnkForRelease = nullptr;

        for (const auto& entry : m_entries)
        {
            if (entry.formatEtc.cfFormat == pformatetcIn->cfFormat &&
                (entry.formatEtc.tymed & pformatetcIn->tymed))
            {
                if (!entry.stgMedium.hGlobal) return E_UNEXPECTED;

                size_t size = GlobalSize(entry.stgMedium.hGlobal);
                HGLOBAL hCopy = GlobalAlloc(GHND | GMEM_SHARE, size);
                if (!hCopy) return E_OUTOFMEMORY;

                void* src = GlobalLock(entry.stgMedium.hGlobal);
                void* dst = GlobalLock(hCopy);
                if (src && dst)
                {
                    memcpy(dst, src, size);
                }
                if (src) GlobalUnlock(entry.stgMedium.hGlobal);
                if (dst) GlobalUnlock(hCopy);

                pmedium->tymed = TYMED_HGLOBAL;
                pmedium->hGlobal = hCopy;
                pmedium->pUnkForRelease = nullptr;
                return S_OK;
            }
        }

        return DV_E_FORMATETC;
    }

    STDMETHODIMP OleDataObject::GetDataHere(FORMATETC* pformatetc, STGMEDIUM* pmedium)
    {
        return E_NOTIMPL;
    }

    STDMETHODIMP OleDataObject::QueryGetData(FORMATETC* pformatetc)
    {
        if (!pformatetc) return E_POINTER;
        for (const auto& entry : m_entries)
        {
            if (entry.formatEtc.cfFormat == pformatetc->cfFormat &&
                (entry.formatEtc.tymed & pformatetc->tymed))
            {
                return S_OK;
            }
        }
        return DV_E_FORMATETC;
    }

    STDMETHODIMP OleDataObject::GetCanonicalFormatEtc(FORMATETC* pformatectIn, FORMATETC* pformatetcOut)
    {
        if (!pformatetcOut) return E_POINTER;
        pformatetcOut->ptd = nullptr;
        return E_NOTIMPL;
    }

    STDMETHODIMP OleDataObject::SetData(FORMATETC* pformatetc, STGMEDIUM* pmedium, BOOL fRelease)
    {
        return E_NOTIMPL;
    }

    STDMETHODIMP OleDataObject::EnumFormatEtc(DWORD dwDirection, IEnumFORMATETC** ppenumFormatEtc)
    {
        if (!ppenumFormatEtc) return E_POINTER;
        *ppenumFormatEtc = nullptr;

        if (dwDirection == DATADIR_GET)
        {
            std::vector<FORMATETC> formats;
            for (const auto& entry : m_entries)
            {
                formats.push_back(entry.formatEtc);
            }
            *ppenumFormatEtc = new EnumFormatEtcImpl(formats);
            return S_OK;
        }

        return E_NOTIMPL;
    }

    STDMETHODIMP OleDataObject::DAdvise(FORMATETC* pformatetc, DWORD advf, IAdviseSink* pAdvSink, DWORD* pdwConnection)
    {
        return OLE_E_ADVISENOTSUPPORTED;
    }

    STDMETHODIMP OleDataObject::DUnadvise(DWORD dwConnection)
    {
        return OLE_E_ADVISENOTSUPPORTED;
    }

    STDMETHODIMP OleDataObject::EnumDAdvise(IEnumSTATDATA** ppenumAdvise)
    {
        return OLE_E_ADVISENOTSUPPORTED;
    }
}
