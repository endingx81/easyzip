#include "AppSettings.h"
#include "PathHelper.h"
#include <shlobj.h>

namespace EasyZip
{
    AppSettings& AppSettings::Instance()
    {
        static AppSettings s_instance;
        return s_instance;
    }

    AppSettings::AppSettings()
    {
        Load();
    }

    std::wstring AppSettings::GetIniFilePath()
    {
        wchar_t appData[MAX_PATH];
        if (SUCCEEDED(SHGetFolderPathW(NULL, CSIDL_LOCAL_APPDATA, NULL, 0, appData)))
        {
            std::wstring dir = PathHelper::Combine(appData, L"EasyZip");
            PathHelper::EnsureDirectoryExists(dir);
            return PathHelper::Combine(dir, L"settings.ini");
        }
        return PathHelper::Combine(PathHelper::GetExeDirectory(), L"settings.ini");
    }

    void AppSettings::Load()
    {
        std::wstring ini = GetIniFilePath();
        if (!PathHelper::FileExists(ini))
        {
            return;
        }

        m_firstRunDone = (GetPrivateProfileIntW(L"General", L"FirstRunDone", 0, ini.c_str()) != 0);
        m_openFolderAfterExtract = (GetPrivateProfileIntW(L"General", L"OpenFolderAfterExtract", 0, ini.c_str()) != 0);
        
        wchar_t buf[64];
        GetPrivateProfileStringW(L"General", L"DefaultFormat", L"zip", buf, _countof(buf), ini.c_str());
        m_defaultFormat = buf;

        m_defaultLevel = GetPrivateProfileIntW(L"General", L"DefaultLevel", 5, ini.c_str());
        m_defaultThreads = GetPrivateProfileIntW(L"General", L"DefaultThreads", 0, ini.c_str());
        m_enableShellMenu = (GetPrivateProfileIntW(L"Shell", L"EnableShellMenu", 1, ini.c_str()) != 0);
        m_showSubmenu = (GetPrivateProfileIntW(L"Shell", L"ShowSubmenu", 0, ini.c_str()) != 0);
        m_menuExtractHere = (GetPrivateProfileIntW(L"Shell", L"MenuExtractHere", 0, ini.c_str()) != 0);
        m_menuExtractSmart = (GetPrivateProfileIntW(L"Shell", L"MenuExtractSmart", 1, ini.c_str()) != 0);
        m_menuExtractFolder = (GetPrivateProfileIntW(L"Shell", L"MenuExtractFolder", 1, ini.c_str()) != 0);
        m_menuExtractEach = (GetPrivateProfileIntW(L"Shell", L"MenuExtractEach", 1, ini.c_str()) != 0);
        m_menuFastZip = (GetPrivateProfileIntW(L"Shell", L"MenuFastZip", 1, ini.c_str()) != 0);
        m_menuCompress = (GetPrivateProfileIntW(L"Shell", L"MenuCompress", 1, ini.c_str()) != 0);
        m_menuNewFolder = (GetPrivateProfileIntW(L"Shell", L"MenuNewFolder", 1, ini.c_str()) != 0);

        wchar_t dirBuf[MAX_PATH * 2];
        GetPrivateProfileStringW(L"General", L"LastExtractDir", L"", dirBuf, _countof(dirBuf), ini.c_str());
        m_lastExtractDir = dirBuf;
    }

    void AppSettings::Save()
    {
        std::wstring ini = GetIniFilePath();

        WritePrivateProfileStringW(L"General", L"FirstRunDone", m_firstRunDone ? L"1" : L"0", ini.c_str());
        WritePrivateProfileStringW(L"General", L"OpenFolderAfterExtract", m_openFolderAfterExtract ? L"1" : L"0", ini.c_str());
        WritePrivateProfileStringW(L"General", L"DefaultFormat", m_defaultFormat.c_str(), ini.c_str());
        WritePrivateProfileStringW(L"General", L"DefaultLevel", std::to_wstring(m_defaultLevel).c_str(), ini.c_str());
        WritePrivateProfileStringW(L"General", L"DefaultThreads", std::to_wstring(m_defaultThreads).c_str(), ini.c_str());
        WritePrivateProfileStringW(L"Shell", L"EnableShellMenu", m_enableShellMenu ? L"1" : L"0", ini.c_str());
        WritePrivateProfileStringW(L"Shell", L"ShowSubmenu", m_showSubmenu ? L"1" : L"0", ini.c_str());
        WritePrivateProfileStringW(L"Shell", L"MenuExtractHere", m_menuExtractHere ? L"1" : L"0", ini.c_str());
        WritePrivateProfileStringW(L"Shell", L"MenuExtractSmart", m_menuExtractSmart ? L"1" : L"0", ini.c_str());
        WritePrivateProfileStringW(L"Shell", L"MenuExtractFolder", m_menuExtractFolder ? L"1" : L"0", ini.c_str());
        WritePrivateProfileStringW(L"Shell", L"MenuExtractEach", m_menuExtractEach ? L"1" : L"0", ini.c_str());
        WritePrivateProfileStringW(L"Shell", L"MenuFastZip", m_menuFastZip ? L"1" : L"0", ini.c_str());
        WritePrivateProfileStringW(L"Shell", L"MenuCompress", m_menuCompress ? L"1" : L"0", ini.c_str());
        WritePrivateProfileStringW(L"Shell", L"MenuNewFolder", m_menuNewFolder ? L"1" : L"0", ini.c_str());
        WritePrivateProfileStringW(L"General", L"LastExtractDir", m_lastExtractDir.c_str(), ini.c_str());

        // Sync to Registry HKCU\Software\EasyZip\Shell for microsecond access in EasyZipShell.dll
        HKEY hKey = NULL;
        if (RegCreateKeyExW(HKEY_CURRENT_USER, L"Software\\EasyZip\\Shell", 0, NULL, 0, KEY_WRITE, NULL, &hKey, NULL) == ERROR_SUCCESS)
        {
            DWORD val = m_enableShellMenu ? 1 : 0;
            RegSetValueExW(hKey, L"EnableShellMenu", 0, REG_DWORD, (const BYTE*)&val, sizeof(val));
            val = m_showSubmenu ? 1 : 0;
            RegSetValueExW(hKey, L"ShowSubmenu", 0, REG_DWORD, (const BYTE*)&val, sizeof(val));
            val = m_menuExtractHere ? 1 : 0;
            RegSetValueExW(hKey, L"MenuExtractHere", 0, REG_DWORD, (const BYTE*)&val, sizeof(val));
            val = m_menuExtractSmart ? 1 : 0;
            RegSetValueExW(hKey, L"MenuExtractSmart", 0, REG_DWORD, (const BYTE*)&val, sizeof(val));
            val = m_menuExtractFolder ? 1 : 0;
            RegSetValueExW(hKey, L"MenuExtractFolder", 0, REG_DWORD, (const BYTE*)&val, sizeof(val));
            val = m_menuExtractEach ? 1 : 0;
            RegSetValueExW(hKey, L"MenuExtractEach", 0, REG_DWORD, (const BYTE*)&val, sizeof(val));
            val = m_menuFastZip ? 1 : 0;
            RegSetValueExW(hKey, L"MenuFastZip", 0, REG_DWORD, (const BYTE*)&val, sizeof(val));
            val = m_menuCompress ? 1 : 0;
            RegSetValueExW(hKey, L"MenuCompress", 0, REG_DWORD, (const BYTE*)&val, sizeof(val));
            val = m_menuNewFolder ? 1 : 0;
            RegSetValueExW(hKey, L"MenuNewFolder", 0, REG_DWORD, (const BYTE*)&val, sizeof(val));
            RegCloseKey(hKey);
        }
    }
}

