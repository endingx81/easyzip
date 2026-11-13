#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef STRICT
#define STRICT
#endif
#include <windows.h>
#include <shellapi.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <string>
#include <vector>
#include <algorithm>

#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "ole32.lib")

// CLSID: {D8F2A385-4C3D-4B9F-8349-3A65C82A4D10}
static const GUID CLSID_EasyZipContextMenu =
{ 0xd8f2a385, 0x4c3d, 0x4b9f, { 0x83, 0x49, 0x3a, 0x65, 0xc8, 0x2a, 0x4d, 0x10 } };

static const wchar_t* CLSID_STRING = L"{D8F2A385-4C3D-4B9F-8349-3A65C82A4D10}";

static HINSTANCE g_hInst = NULL;
static long g_cRef = 0;

static std::wstring GetFileNameWithoutExtension(const std::wstring& path)
{
    size_t lastSlash = path.find_last_of(L"\\/");
    std::wstring name = (lastSlash != std::wstring::npos) ? path.substr(lastSlash + 1) : path;
    size_t lastDot = name.find_last_of(L'.');
    if (lastDot != std::wstring::npos && lastDot > 0)
    {
        std::wstring base = name.substr(0, lastDot);
        size_t secondDot = base.find_last_of(L'.');
        if (secondDot != std::wstring::npos && secondDot > 0)
        {
            std::wstring subExt = base.substr(secondDot);
            std::wstring lowerSub = subExt;
            std::transform(lowerSub.begin(), lowerSub.end(), lowerSub.begin(), ::towlower);
            if (lowerSub == L".tar" || lowerSub == L".7z" || lowerSub == L".zip")
            {
                return base.substr(0, secondDot);
            }
        }
        return base;
    }
    return name;
}

static bool IsArchiveFile(const std::wstring& path)
{
    size_t dot = path.find_last_of(L'.');
    if (dot == std::wstring::npos) return false;
    std::wstring ext = path.substr(dot);
    std::transform(ext.begin(), ext.end(), ext.begin(), ::towlower);

    static const wchar_t* exts[] = {
        L".zip", L".7z", L".rar", L".tar", L".gz", L".tgz",
        L".bz2", L".tbz2", L".xz", L".txz", L".iso", L".cab",
        L".zst", L".tzst", L".lz", L".lzh", L".alz", L".egg",
        L".001"
    };

    for (const auto& e : exts)
    {
        if (ext == e) return true;
    }
    return false;
}

static std::wstring GetExePath()
{
    wchar_t buf[MAX_PATH] = { 0 };
    GetModuleFileNameW(g_hInst, buf, MAX_PATH);
    PathRemoveFileSpecW(buf);
    PathAppendW(buf, L"EasyZip.exe");
    if (PathFileExistsW(buf)) return buf;

    wchar_t localApp[MAX_PATH] = { 0 };
    if (GetEnvironmentVariableW(L"LOCALAPPDATA", localApp, MAX_PATH) > 0)
    {
        PathAppendW(localApp, L"Programs\\EasyZip\\EasyZip.exe");
        if (PathFileExistsW(localApp)) return localApp;
    }

    return L"EasyZip.exe";
}

static std::wstring GetParentDirectoryName(const std::wstring& path)
{
    size_t lastSlash = path.find_last_of(L"\\/");
    if (lastSlash != std::wstring::npos && lastSlash > 0)
    {
        std::wstring parent = path.substr(0, lastSlash);
        size_t prevSlash = parent.find_last_of(L"\\/");
        if (prevSlash != std::wstring::npos)
        {
            return parent.substr(prevSlash + 1);
        }
        return parent;
    }
    return L"";
}

static HBITMAP CreateMenuBitmapFromIcon(HICON hIcon, int cx = 16, int cy = 16)
{
    if (!hIcon) return NULL;

    HDC hdcScreen = GetDC(NULL);
    HDC hdcMem = CreateCompatibleDC(hdcScreen);

    BITMAPINFO bi = { 0 };
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = cx;
    bi.bmiHeader.biHeight = -cy; // top-down
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;

    RGBQUAD* pPixels = NULL;
    HBITMAP hBmp = CreateDIBSection(hdcMem, &bi, DIB_RGB_COLORS, (void**)&pPixels, NULL, 0);
    if (!hBmp)
    {
        DeleteDC(hdcMem);
        ReleaseDC(NULL, hdcScreen);
        return NULL;
    }

    HBITMAP hOldBmp = (HBITMAP)SelectObject(hdcMem, hBmp);

    // Alpha channel 투명 초기화
    memset(pPixels, 0, cx * cy * sizeof(RGBQUAD));

    DrawIconEx(hdcMem, 0, 0, hIcon, cx, cy, 0, NULL, DI_NORMAL);

    SelectObject(hdcMem, hOldBmp);
    DeleteDC(hdcMem);
    ReleaseDC(NULL, hdcScreen);

    return hBmp;
}

enum ShellAction
{
    ACTION_EXTRACT_HERE = 0,
    ACTION_EXTRACT_SMART = 1,
    ACTION_EXTRACT_FOLDER = 2,
    ACTION_EXTRACT_EACH = 3,
    ACTION_OPEN = 4,
    ACTION_FAST_ZIP = 5,
    ACTION_COMPRESS = 6,
    ACTION_NEW_FOLDER = 7
};

struct ShellSettings
{
    bool enableShell = true;
    bool showSubmenu = false;
    bool newFolder = true;
    bool extractHere = false;
    bool extractSmart = true;
    bool extractFolder = true;
    bool extractEach = true;
    bool fastZip = true;
    bool compress = true;
};

static ShellSettings LoadShellSettings()
{
    ShellSettings s;
    HKEY hKey = NULL;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\EasyZip\\Shell", 0, KEY_READ, &hKey) == ERROR_SUCCESS)
    {
        auto readBool = [&](const wchar_t* name, bool defVal) -> bool {
            DWORD val = 0;
            DWORD size = sizeof(val);
            if (RegQueryValueExW(hKey, name, NULL, NULL, (LPBYTE)&val, &size) == ERROR_SUCCESS)
            {
                return val != 0;
            }
            return defVal;
        };

        s.enableShell = readBool(L"EnableShellMenu", true);
        s.showSubmenu = readBool(L"ShowSubmenu", false);
        s.newFolder = readBool(L"MenuNewFolder", true);
        s.extractHere = readBool(L"MenuExtractHere", false);
        s.extractSmart = readBool(L"MenuExtractSmart", true);
        s.extractFolder = readBool(L"MenuExtractFolder", true);
        s.extractEach = readBool(L"MenuExtractEach", true);
        s.fastZip = readBool(L"MenuFastZip", true);
        s.compress = readBool(L"MenuCompress", true);

        RegCloseKey(hKey);
    }
    return s;
}

class CEasyZipContextMenu : public IShellExtInit, public IContextMenu3
{
private:
    long m_cRef;
    std::vector<std::wstring> m_files;
    std::wstring m_folderBackground;
    bool m_allArchives;
    std::vector<int> m_cmdMap;
    HBITMAP m_hBitmap;

public:
    CEasyZipContextMenu() : m_cRef(1), m_allArchives(false), m_hBitmap(NULL)
    {
        InterlockedIncrement(&g_cRef);
        std::wstring exe = GetExePath();
        HICON hIcon = NULL;
        if (ExtractIconExW(exe.c_str(), 0, NULL, &hIcon, 1) > 0 && hIcon)
        {
            m_hBitmap = CreateMenuBitmapFromIcon(hIcon, 16, 16);
            DestroyIcon(hIcon);
        }
    }

    ~CEasyZipContextMenu()
    {
        if (m_hBitmap) DeleteObject(m_hBitmap);
        InterlockedDecrement(&g_cRef);
    }

    // IUnknown
    STDMETHODIMP QueryInterface(REFIID riid, void** ppv) override
    {
        if (!ppv) return E_POINTER;
        *ppv = NULL;

        if (IsEqualIID(riid, IID_IUnknown) || IsEqualIID(riid, IID_IShellExtInit))
        {
            *ppv = static_cast<IShellExtInit*>(this);
        }
        else if (IsEqualIID(riid, IID_IContextMenu) || IsEqualIID(riid, IID_IContextMenu2) || IsEqualIID(riid, IID_IContextMenu3))
        {
            *ppv = static_cast<IContextMenu3*>(this);
        }
        else
        {
            return E_NOINTERFACE;
        }

        AddRef();
        return S_OK;
    }

    STDMETHODIMP_(ULONG) AddRef() override
    {
        return InterlockedIncrement(&m_cRef);
    }

    STDMETHODIMP_(ULONG) Release() override
    {
        long c = InterlockedDecrement(&m_cRef);
        if (c == 0) delete this;
        return c;
    }

    // IShellExtInit
    STDMETHODIMP Initialize(PCIDLIST_ABSOLUTE pidlFolder, IDataObject* pdtobj, HKEY hkeyProgID) override
    {
        m_files.clear();
        m_folderBackground.clear();
        m_allArchives = false;

        if (pdtobj)
        {
            FORMATETC etc = { CF_HDROP, NULL, DVASPECT_CONTENT, -1, TYMED_HGLOBAL };
            STGMEDIUM stg;
            if (SUCCEEDED(pdtobj->GetData(&etc, &stg)))
            {
                HDROP hDrop = (HDROP)GlobalLock(stg.hGlobal);
                if (hDrop)
                {
                    UINT count = DragQueryFileW(hDrop, 0xFFFFFFFF, NULL, 0);
                    for (UINT i = 0; i < count; i++)
                    {
                        wchar_t path[MAX_PATH * 2] = { 0 };
                        if (DragQueryFileW(hDrop, i, path, MAX_PATH * 2) > 0)
                        {
                            m_files.push_back(path);
                        }
                    }
                    GlobalUnlock(stg.hGlobal);
                }
                ReleaseStgMedium(&stg);
            }
        }

        if (pidlFolder)
        {
            wchar_t bgPath[MAX_PATH * 2] = { 0 };
            if (SHGetPathFromIDListW(pidlFolder, bgPath))
            {
                m_folderBackground = bgPath;
            }
        }

        if (!m_files.empty())
        {
            m_allArchives = true;
            for (const auto& f : m_files)
            {
                if (!IsArchiveFile(f))
                {
                    m_allArchives = false;
                    break;
                }
            }
        }

        return S_OK;
    }

    // IContextMenu
    STDMETHODIMP QueryContextMenu(HMENU hmenu, UINT indexMenu, UINT idCmdFirst, UINT idCmdLast, UINT uFlags) override
    {
        if (uFlags & CMF_DEFAULTONLY)
        {
            return MAKE_HRESULT(SEVERITY_SUCCESS, 0, 0);
        }

        // 파일이 선택되지 않은 빈 공간(바탕화면/폴더 배경)은 정적 Position="Top" 동사가 전담하므로 조기 반환
        if (m_files.empty()) return MAKE_HRESULT(SEVERITY_SUCCESS, 0, 0);

        ShellSettings s = LoadShellSettings();
        if (!s.enableShell) return MAKE_HRESULT(SEVERITY_SUCCESS, 0, 0);

        m_cmdMap.clear();

        struct MenuItemDef
        {
            std::wstring text;
            ShellAction action;
        };
        std::vector<MenuItemDef> items;

        if (!m_files.empty())
        {
            if (m_allArchives)
            {
                // 1. 여기에 압축 풀기(&X) - 설정 시 최상단
                if (s.extractHere)
                {
                    items.push_back({ L"여기에 압축 풀기(&X)", ACTION_EXTRACT_HERE });
                }

                // 2. 알아서 풀기(&A)
                if (s.extractSmart)
                {
                    items.push_back({ L"알아서 풀기(&A)", ACTION_EXTRACT_SMART });
                }

                // 3. 파일명 풀기: 단일 vs 다중 분기
                if (m_files.size() == 1)
                {
                    if (s.extractFolder)
                    {
                        std::wstring baseName = GetFileNameWithoutExtension(m_files[0]);
                        items.push_back({ L"\"" + baseName + L"\\\"에 압축 풀기(&E)", ACTION_EXTRACT_FOLDER });
                    }
                    items.push_back({ L"EasyZip으로 열기(&O)", ACTION_OPEN });
                }
                else
                {
                    if (s.extractEach)
                    {
                        items.push_back({ L"각각의 폴더에 압축 풀기(&E)", ACTION_EXTRACT_EACH });
                    }
                }
            }
            else
            {
                // 일반 파일 및 폴더인 경우:
                // 1. 빠른 ZIP 압축
                if (s.fastZip)
                {
                    std::wstring strQuickZip;
                    if (m_files.size() == 1)
                    {
                        std::wstring baseName = GetFileNameWithoutExtension(m_files[0]);
                        strQuickZip = L"\"" + baseName + L".zip\"으로 빠른 압축(&Z)";
                    }
                    else
                    {
                        std::wstring parentName = GetParentDirectoryName(m_files[0]);
                        if (parentName.empty() || parentName.find(L':') != std::wstring::npos)
                        {
                            parentName = GetFileNameWithoutExtension(m_files[0]);
                        }
                        if (parentName.empty()) parentName = L"Archive";
                        strQuickZip = L"\"" + parentName + L".zip\"으로 빠른 압축(&Z)";
                    }
                    items.push_back({ strQuickZip, ACTION_FAST_ZIP });
                }

                // 2. 압축하기...
                if (s.compress)
                {
                    items.push_back({ L"EasyZip으로 압축하기(&C)...", ACTION_COMPRESS });
                }
            }
        }

        if (items.empty())
        {
            return MAKE_HRESULT(SEVERITY_SUCCESS, 0, 0);
        }

        UINT id = idCmdFirst;

        if (s.showSubmenu)
        {
            HMENU hSubMenu = CreatePopupMenu();
            UINT subIndex = 0;
            for (const auto& it : items)
            {
                InsertMenuItemCustom(hSubMenu, subIndex++, id++, it.text.c_str());
                m_cmdMap.push_back(it.action);
            }

            MENUITEMINFOW mii = { sizeof(mii) };
            mii.fMask = MIIM_SUBMENU | MIIM_STRING | MIIM_ID;
            mii.wID = id++;
            mii.hSubMenu = hSubMenu;
            mii.dwTypeData = const_cast<wchar_t*>(L"EasyZip");

            if (m_hBitmap)
            {
                mii.fMask |= MIIM_BITMAP;
                mii.hbmpItem = m_hBitmap;
            }

            InsertMenuItemW(hmenu, indexMenu++, TRUE, &mii);
        }
        else
        {
            for (const auto& it : items)
            {
                InsertMenuItemCustom(hmenu, indexMenu++, id++, it.text.c_str());
                m_cmdMap.push_back(it.action);
            }
        }

        return MAKE_HRESULT(SEVERITY_SUCCESS, 0, (USHORT)(id - idCmdFirst));
    }

    STDMETHODIMP InvokeCommand(CMINVOKECOMMANDINFO* pici) override
    {
        if (!pici) return E_INVALIDARG;

        if (HIWORD(pici->lpVerb) != 0) return E_FAIL;

        UINT index = LOWORD(pici->lpVerb);
        if (index >= m_cmdMap.size()) return E_FAIL;

        int actionId = m_cmdMap[index];
        if (actionId == ACTION_NEW_FOLDER)
        {
            std::wstring targetDir;
            if (!m_folderBackground.empty())
            {
                targetDir = m_folderBackground;
            }
            else if (!m_files.empty())
            {
                targetDir = m_files[0];
                DWORD attr = GetFileAttributesW(targetDir.c_str());
                if (attr != INVALID_FILE_ATTRIBUTES && !(attr & FILE_ATTRIBUTE_DIRECTORY))
                {
                    size_t pos = targetDir.find_last_of(L"\\/");
                    if (pos != std::wstring::npos)
                    {
                        targetDir = targetDir.substr(0, pos);
                    }
                }
                else if (attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY))
                {
                    size_t pos = targetDir.find_last_of(L"\\/");
                    if (pos != std::wstring::npos && pos > 2)
                    {
                        targetDir = targetDir.substr(0, pos);
                    }
                }
            }

            // Fallback to Desktop
            if (targetDir.empty())
            {
                wchar_t deskPath[MAX_PATH] = { 0 };
                if (SHGetSpecialFolderPathW(NULL, deskPath, CSIDL_DESKTOPDIRECTORY, FALSE))
                {
                    targetDir = deskPath;
                }
            }

            if (targetDir.empty()) return E_FAIL;
            if (targetDir.back() != L'\\' && targetDir.back() != L'/')
            {
                targetDir += L'\\';
            }

            std::wstring newFolder = targetDir + L"새 폴더";
            int counter = 2;
            while (PathFileExistsW(newFolder.c_str()))
            {
                newFolder = targetDir + L"새 폴더 (" + std::to_wstring(counter++) + L")";
            }

            if (CreateDirectoryW(newFolder.c_str(), NULL))
            {
                // 새 탐색기 창을 강제로 팝업시키는 SHOpenFolderAndSelectItems 배제, 조용히 현재 창만 갱신
                SHChangeNotify(SHCNE_MKDIR, SHCNF_PATHW, newFolder.c_str(), NULL);
                SHChangeNotify(SHCNE_UPDATEDIR, SHCNF_PATHW, targetDir.c_str(), NULL);
                return S_OK;
            }
            return E_FAIL;
        }

        std::wstring exe = GetExePath();

        std::wstring args;
        switch (actionId)
        {
        case ACTION_EXTRACT_HERE:
            args = L"-x";
            break;
        case ACTION_EXTRACT_SMART:
            args = L"-xs";
            break;
        case ACTION_EXTRACT_FOLDER:
        case ACTION_EXTRACT_EACH:
            args = L"-xd";
            break;
        case ACTION_OPEN:
            args = L"";
            break;
        case ACTION_COMPRESS:
            args = L"-c";
            break;
        case ACTION_FAST_ZIP:
            args = L"-cz";
            break;
        default:
            return E_FAIL;
        }

        std::wstring cmdLine = L"\"" + exe + L"\"";
        if (!args.empty())
        {
            cmdLine += L" " + args;
        }

        for (const auto& f : m_files)
        {
            cmdLine += L" \"" + f + L"\"";
        }

        STARTUPINFOW si = { sizeof(si) };
        PROCESS_INFORMATION pi = { 0 };

        std::vector<wchar_t> cmdBuf(cmdLine.begin(), cmdLine.end());
        cmdBuf.push_back(L'\0');

        if (CreateProcessW(NULL, cmdBuf.data(), NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi))
        {
            CloseHandle(pi.hThread);
            CloseHandle(pi.hProcess);
            return S_OK;
        }

        return E_FAIL;
    }

    STDMETHODIMP GetCommandString(UINT_PTR idCmd, UINT uFlags, UINT* pwReserved, LPSTR pszName, UINT cchMax) override
    {
        return E_NOTIMPL;
    }

    // IContextMenu2
    STDMETHODIMP HandleMenuMsg(UINT uMsg, WPARAM wParam, LPARAM lParam) override
    {
        return S_OK;
    }

    // IContextMenu3
    STDMETHODIMP HandleMenuMsg2(UINT uMsg, WPARAM wParam, LPARAM lParam, LRESULT* plResult) override
    {
        if (plResult) *plResult = 0;
        return S_OK;
    }

private:
    void InsertMenuItemCustom(HMENU hmenu, UINT index, UINT id, const wchar_t* text)
    {
        MENUITEMINFOW mii = { sizeof(mii) };
        mii.fMask = MIIM_ID | MIIM_STRING | MIIM_FTYPE;
        mii.fType = MFT_STRING;
        mii.wID = id;
        mii.dwTypeData = const_cast<wchar_t*>(text);

        if (m_hBitmap)
        {
            mii.fMask |= MIIM_BITMAP;
            mii.hbmpItem = m_hBitmap;
        }

        if (!InsertMenuItemW(hmenu, index, TRUE, &mii))
        {
            // 비트맵 호환 문제로 실패 시 순수 텍스트로 즉시 재시도 (누락 방어)
            mii.fMask = MIIM_ID | MIIM_STRING | MIIM_FTYPE;
            mii.hbmpItem = NULL;
            InsertMenuItemW(hmenu, index, TRUE, &mii);
        }
    }
};

class CClassFactory : public IClassFactory
{
private:
    long m_cRef;

public:
    CClassFactory() : m_cRef(1)
    {
        InterlockedIncrement(&g_cRef);
    }

    ~CClassFactory()
    {
        InterlockedDecrement(&g_cRef);
    }

    STDMETHODIMP QueryInterface(REFIID riid, void** ppv) override
    {
        if (!ppv) return E_POINTER;
        *ppv = NULL;

        if (IsEqualIID(riid, IID_IUnknown) || IsEqualIID(riid, IID_IClassFactory))
        {
            *ppv = static_cast<IClassFactory*>(this);
            AddRef();
            return S_OK;
        }
        return E_NOINTERFACE;
    }

    STDMETHODIMP_(ULONG) AddRef() override
    {
        return InterlockedIncrement(&m_cRef);
    }

    STDMETHODIMP_(ULONG) Release() override
    {
        long c = InterlockedDecrement(&m_cRef);
        if (c == 0) delete this;
        return c;
    }

    STDMETHODIMP CreateInstance(IUnknown* pUnkOuter, REFIID riid, void** ppv) override
    {
        if (pUnkOuter) return CLASS_E_NOAGGREGATION;
        CEasyZipContextMenu* pObj = new (std::nothrow) CEasyZipContextMenu();
        if (!pObj) return E_OUTOFMEMORY;
        HRESULT hr = pObj->QueryInterface(riid, ppv);
        pObj->Release();
        return hr;
    }

    STDMETHODIMP LockServer(BOOL fLock) override
    {
        if (fLock) InterlockedIncrement(&g_cRef);
        else InterlockedDecrement(&g_cRef);
        return S_OK;
    }
};

BOOL WINAPI DllMain(HINSTANCE hinst, DWORD reason, LPVOID reserved)
{
    if (reason == DLL_PROCESS_ATTACH)
    {
        g_hInst = hinst;
        DisableThreadLibraryCalls(hinst);
    }
    return TRUE;
}

STDAPI DllCanUnloadNow(void)
{
    return (g_cRef == 0) ? S_OK : S_FALSE;
}

STDAPI DllGetClassObject(REFCLSID rclsid, REFIID riid, void** ppv)
{
    if (!ppv) return E_POINTER;
    *ppv = NULL;

    if (IsEqualCLSID(rclsid, CLSID_EasyZipContextMenu))
    {
        CClassFactory* pFactory = new (std::nothrow) CClassFactory();
        if (!pFactory) return E_OUTOFMEMORY;
        HRESULT hr = pFactory->QueryInterface(riid, ppv);
        pFactory->Release();
        return hr;
    }

    return CLASS_E_CLASSNOTAVAILABLE;
}

// HKCU Registry Helper
static LONG SetRegString(HKEY hKeyRoot, const wchar_t* subKey, const wchar_t* valName, const wchar_t* data)
{
    HKEY hKey;
    LONG res = RegCreateKeyExW(hKeyRoot, subKey, 0, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hKey, NULL);
    if (res == ERROR_SUCCESS)
    {
        res = RegSetValueExW(hKey, valName, 0, REG_SZ, (const BYTE*)data, (DWORD)((wcslen(data) + 1) * sizeof(wchar_t)));
        RegCloseKey(hKey);
    }
    return res;
}

STDAPI DllRegisterServer(void)
{
    wchar_t dllPath[MAX_PATH] = { 0 };
    GetModuleFileNameW(g_hInst, dllPath, MAX_PATH);

    std::wstring clsidKey = L"Software\\Classes\\CLSID\\" + std::wstring(CLSID_STRING);
    SetRegString(HKEY_CURRENT_USER, clsidKey.c_str(), NULL, L"EasyZip Shell Extension");
    SetRegString(HKEY_CURRENT_USER, (clsidKey + L"\\InprocServer32").c_str(), NULL, dllPath);
    SetRegString(HKEY_CURRENT_USER, (clsidKey + L"\\InprocServer32").c_str(), L"ThreadingModel", L"Apartment");

    // ContextMenuHandlers
    SetRegString(HKEY_CURRENT_USER, L"Software\\Classes\\*\\shellex\\ContextMenuHandlers\\EasyZip", NULL, CLSID_STRING);
    SetRegString(HKEY_CURRENT_USER, L"Software\\Classes\\Directory\\shellex\\ContextMenuHandlers\\EasyZip", NULL, CLSID_STRING);
    SetRegString(HKEY_CURRENT_USER, L"Software\\Classes\\Directory\\Background\\shellex\\ContextMenuHandlers\\EasyZip", NULL, CLSID_STRING);
    SetRegString(HKEY_CURRENT_USER, L"Software\\Classes\\DesktopBackground\\shellex\\ContextMenuHandlers\\EasyZip", NULL, CLSID_STRING);
    SetRegString(HKEY_CURRENT_USER, L"Software\\Classes\\Drive\\shellex\\ContextMenuHandlers\\EasyZip", NULL, CLSID_STRING);
    SetRegString(HKEY_CURRENT_USER, L"Software\\Classes\\Folder\\shellex\\ContextMenuHandlers\\EasyZip", NULL, CLSID_STRING);
    SetRegString(HKEY_CURRENT_USER, L"Software\\Classes\\EasyZip.Archive\\shellex\\ContextMenuHandlers\\EasyZip", NULL, CLSID_STRING);

    // Clean old static shell verbs
    RegDeleteTreeW(HKEY_CURRENT_USER, L"Software\\Classes\\EasyZip.Archive\\shell\\ExtractHere");
    RegDeleteTreeW(HKEY_CURRENT_USER, L"Software\\Classes\\EasyZip.Archive\\shell\\ExtractSmart");
    RegDeleteTreeW(HKEY_CURRENT_USER, L"Software\\Classes\\EasyZip.Archive\\shell\\ExtractFolder");
    RegDeleteTreeW(HKEY_CURRENT_USER, L"Software\\Classes\\*\\shell\\EasyZip.Compress");
    RegDeleteTreeW(HKEY_CURRENT_USER, L"Software\\Classes\\*\\shell\\EasyZip.FastZip");
    RegDeleteTreeW(HKEY_CURRENT_USER, L"Software\\Classes\\Directory\\shell\\EasyZip.Compress");
    RegDeleteTreeW(HKEY_CURRENT_USER, L"Software\\Classes\\Directory\\shell\\EasyZip.FastZip");

    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, NULL, NULL);
    return S_OK;
}

STDAPI DllUnregisterServer(void)
{
    std::wstring clsidKey = L"Software\\Classes\\CLSID\\" + std::wstring(CLSID_STRING);
    RegDeleteTreeW(HKEY_CURRENT_USER, clsidKey.c_str());

    RegDeleteTreeW(HKEY_CURRENT_USER, L"Software\\Classes\\*\\shellex\\ContextMenuHandlers\\EasyZip");
    RegDeleteTreeW(HKEY_CURRENT_USER, L"Software\\Classes\\Directory\\shellex\\ContextMenuHandlers\\EasyZip");
    RegDeleteTreeW(HKEY_CURRENT_USER, L"Software\\Classes\\Directory\\Background\\shellex\\ContextMenuHandlers\\EasyZip");
    RegDeleteTreeW(HKEY_CURRENT_USER, L"Software\\Classes\\DesktopBackground\\shellex\\ContextMenuHandlers\\EasyZip");
    RegDeleteTreeW(HKEY_CURRENT_USER, L"Software\\Classes\\Drive\\shellex\\ContextMenuHandlers\\EasyZip");
    RegDeleteTreeW(HKEY_CURRENT_USER, L"Software\\Classes\\Folder\\shellex\\ContextMenuHandlers\\EasyZip");
    RegDeleteTreeW(HKEY_CURRENT_USER, L"Software\\Classes\\EasyZip.Archive\\shellex\\ContextMenuHandlers\\EasyZip");

    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, NULL, NULL);
    return S_OK;
}
