#include "FileAssociation.h"
#include "PathHelper.h"
#include "AppSettings.h"
#include <shellapi.h>
#include <shlobj.h>
#include <shobjidl.h>

namespace EasyZip
{
    const std::vector<std::wstring>& FileAssociation::GetSupportedExtensions()
    {
        static const std::vector<std::wstring> s_exts = {
            L".zip", L".7z", L".rar", L".alz", L".egg",
            L".tar", L".gz", L".tgz", L".bz2", L".tbz2",
            L".xz", L".txz", L".iso", L".cab", L".001"
        };
        return s_exts;
    }

    // Helper: Register a static shell verb under a registry key
    static void RegisterShellVerb(const std::wstring& rootSubKey, const std::wstring& verb, const std::wstring& menuText, const std::wstring& command, const std::wstring& iconPath)
    {
        std::wstring verbPath = rootSubKey + L"\\shell\\" + verb;
        HKEY hVerbKey;
        if (RegCreateKeyExW(HKEY_CURRENT_USER, verbPath.c_str(), 0, NULL, 0, KEY_WRITE, NULL, &hVerbKey, NULL) == ERROR_SUCCESS)
        {
            RegSetValueExW(hVerbKey, NULL, 0, REG_SZ, reinterpret_cast<const BYTE*>(menuText.c_str()), static_cast<DWORD>((menuText.length() + 1) * sizeof(wchar_t)));
            if (!iconPath.empty())
            {
                RegSetValueExW(hVerbKey, L"Icon", 0, REG_SZ, reinterpret_cast<const BYTE*>(iconPath.c_str()), static_cast<DWORD>((iconPath.length() + 1) * sizeof(wchar_t)));
            }

            HKEY hCmdKey;
            if (RegCreateKeyExW(hVerbKey, L"command", 0, NULL, 0, KEY_WRITE, NULL, &hCmdKey, NULL) == ERROR_SUCCESS)
            {
                RegSetValueExW(hCmdKey, NULL, 0, REG_SZ, reinterpret_cast<const BYTE*>(command.c_str()), static_cast<DWORD>((command.length() + 1) * sizeof(wchar_t)));
                RegCloseKey(hCmdKey);
            }
            RegCloseKey(hVerbKey);
        }
    }

    // Clean legacy COM DLL shell extension keys (Purge EasyZipShell.dll registration)
    static void CleanLegacyShellExtension()
    {
        static const wchar_t* legacyHandlers[] = {
            L"Software\\Classes\\*\\shellex\\ContextMenuHandlers\\EasyZip",
            L"Software\\Classes\\Directory\\shellex\\ContextMenuHandlers\\EasyZip",
            L"Software\\Classes\\Directory\\Background\\shellex\\ContextMenuHandlers\\EasyZip",
            L"Software\\Classes\\DesktopBackground\\shellex\\ContextMenuHandlers\\EasyZip",
            L"Software\\Classes\\Drive\\shellex\\ContextMenuHandlers\\EasyZip",
            L"Software\\Classes\\Folder\\shellex\\ContextMenuHandlers\\EasyZip",
            L"Software\\Classes\\EasyZip.Archive\\shellex\\ContextMenuHandlers\\EasyZip",
            L"Software\\Classes\\CLSID\\{D8F2A385-4C3D-4B9F-8349-3A65C82A4D10}"
        };

        const HKEY roots[] = { HKEY_CURRENT_USER, HKEY_LOCAL_MACHINE };
        for (HKEY root : roots)
        {
            for (const auto* k : legacyHandlers)
            {
                RegDeleteTreeW(root, k);
            }

            // Remove from Shell Extensions\Approved
            HKEY hAppr = NULL;
            if (RegOpenKeyExW(root, L"Software\\Microsoft\\Windows\\CurrentVersion\\Shell Extensions\\Approved", 0, KEY_SET_VALUE, &hAppr) == ERROR_SUCCESS)
            {
                RegDeleteValueW(hAppr, L"{D8F2A385-4C3D-4B9F-8349-3A65C82A4D10}");
                RegCloseKey(hAppr);
            }
        }
    }

    static void CleanStaticShellVerbs()
    {
        RegDeleteTreeW(HKEY_CURRENT_USER, L"Software\\Classes\\*\\shell\\EasyZip.Compress");
        RegDeleteTreeW(HKEY_CURRENT_USER, L"Software\\Classes\\*\\shell\\EasyZip.FastZip");
        RegDeleteTreeW(HKEY_CURRENT_USER, L"Software\\Classes\\Directory\\shell\\EasyZip.Compress");
        RegDeleteTreeW(HKEY_CURRENT_USER, L"Software\\Classes\\Directory\\shell\\EasyZip.FastZip");
        RegDeleteTreeW(HKEY_CURRENT_USER, L"Software\\Classes\\Folder\\shell\\EasyZip.Compress");
        RegDeleteTreeW(HKEY_CURRENT_USER, L"Software\\Classes\\Folder\\shell\\EasyZip.FastZip");
    }

    static void RegisterCompressVerbs(bool enable)
    {
        CleanStaticShellVerbs();
        if (!enable) return;

        std::wstring exePath = PathHelper::GetExePath();
        std::wstring iconVal = L"\"" + exePath + L"\",0";

        // 1. Files: HKCU\Software\Classes\*\shell\EasyZip.Compress
        RegisterShellVerb(
            L"Software\\Classes\\*",
            L"EasyZip.Compress",
            L"EasyZip으로 압축하기(&C)...",
            L"\"" + exePath + L"\" -c \"%1\"",
            iconVal
        );

        // 2. Directories: HKCU\Software\Classes\Directory\shell\EasyZip.Compress
        RegisterShellVerb(
            L"Software\\Classes\\Directory",
            L"EasyZip.Compress",
            L"EasyZip으로 압축하기(&C)...",
            L"\"" + exePath + L"\" -c \"%V\"",
            iconVal
        );
    }

    static void RegisterSingleProgId(const std::wstring& progId, const std::wstring& desc, const std::wstring& exePath)
    {
        HKEY hProgKey;
        std::wstring progKeyPath = L"Software\\Classes\\" + progId;
        if (RegCreateKeyExW(HKEY_CURRENT_USER, progKeyPath.c_str(), 0, NULL, 0, KEY_WRITE, NULL, &hProgKey, NULL) == ERROR_SUCCESS)
        {
            RegSetValueExW(hProgKey, NULL, 0, REG_SZ, reinterpret_cast<const BYTE*>(desc.c_str()), static_cast<DWORD>((desc.length() + 1) * sizeof(wchar_t)));
            RegSetValueExW(hProgKey, L"FriendlyTypeName", 0, REG_SZ, reinterpret_cast<const BYTE*>(desc.c_str()), static_cast<DWORD>((desc.length() + 1) * sizeof(wchar_t)));

            // DefaultIcon
            std::wstring iconVal = L"\"" + exePath + L"\",0";
            HKEY hIconKey;
            if (RegCreateKeyExW(hProgKey, L"DefaultIcon", 0, NULL, 0, KEY_WRITE, NULL, &hIconKey, NULL) == ERROR_SUCCESS)
            {
                RegSetValueExW(hIconKey, NULL, 0, REG_SZ, reinterpret_cast<const BYTE*>(iconVal.c_str()), static_cast<DWORD>((iconVal.length() + 1) * sizeof(wchar_t)));
                RegCloseKey(hIconKey);
            }

            // 1. shell\open (기본 더블클릭 실행 동사)
            HKEY hCmdKey;
            if (RegCreateKeyExW(hProgKey, L"shell\\open\\command", 0, NULL, 0, KEY_WRITE, NULL, &hCmdKey, NULL) == ERROR_SUCCESS)
            {
                std::wstring cmdVal = L"\"" + exePath + L"\" \"%1\"";
                RegSetValueExW(hCmdKey, NULL, 0, REG_SZ, reinterpret_cast<const BYTE*>(cmdVal.c_str()), static_cast<DWORD>((cmdVal.length() + 1) * sizeof(wchar_t)));
                RegCloseKey(hCmdKey);
            }

            HKEY hOpenKey;
            if (RegOpenKeyExW(hProgKey, L"shell\\open", 0, KEY_WRITE, &hOpenKey) == ERROR_SUCCESS)
            {
                std::wstring appName = L"열기(&O)";
                RegSetValueExW(hOpenKey, NULL, 0, REG_SZ, reinterpret_cast<const BYTE*>(appName.c_str()), static_cast<DWORD>((appName.length() + 1) * sizeof(wchar_t)));
                RegSetValueExW(hOpenKey, L"Icon", 0, REG_SZ, reinterpret_cast<const BYTE*>(iconVal.c_str()), static_cast<DWORD>((iconVal.length() + 1) * sizeof(wchar_t)));
                RegCloseKey(hOpenKey);
            }

            // 2. shell\ExtractSmart: 알아서 풀기(&A) (100% 무조건 동작하는 순수 정적 동사!)
            if (AppSettings::Instance().GetMenuExtractSmart())
            {
                RegisterShellVerb(progKeyPath, L"ExtractSmart", L"알아서 풀기(&A)", L"\"" + exePath + L"\" -xs \"%1\"", iconVal);
            }
            else
            {
                RegDeleteTreeW(hProgKey, L"shell\\ExtractSmart");
            }

            // 3. shell\ExtractHere: 여기에 압축 풀기(&X)
            if (AppSettings::Instance().GetMenuExtractHere())
            {
                RegisterShellVerb(progKeyPath, L"ExtractHere", L"여기에 압축 풀기(&X)", L"\"" + exePath + L"\" -x \"%1\"", iconVal);
            }
            else
            {
                RegDeleteTreeW(hProgKey, L"shell\\ExtractHere");
            }

            // 4. shell\ExtractFolder: 폴더에 압축 풀기(&E)
            if (AppSettings::Instance().GetMenuExtractFolder())
            {
                RegisterShellVerb(progKeyPath, L"ExtractFolder", L"폴더에 압축 풀기(&E)", L"\"" + exePath + L"\" -xd \"%1\"", iconVal);
            }
            else
            {
                RegDeleteTreeW(hProgKey, L"shell\\ExtractFolder");
            }

            // 구형 COM 셸 확장 핸들러 제거 (중복 및 충돌 방지)
            RegDeleteTreeW(hProgKey, L"shellex\\ContextMenuHandlers\\EasyZip");

            RegCloseKey(hProgKey);
        }
    }

    static void RegisterApplicationInfo(const std::wstring& exePath)
    {
        // 0. HKCU\Software\Microsoft\Windows\CurrentVersion\App Paths\EasyZip.exe
        HKEY hAppPathsKey;
        if (RegCreateKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\App Paths\\EasyZip.exe", 0, NULL, 0, KEY_WRITE, NULL, &hAppPathsKey, NULL) == ERROR_SUCCESS)
        {
            RegSetValueExW(hAppPathsKey, NULL, 0, REG_SZ, reinterpret_cast<const BYTE*>(exePath.c_str()), static_cast<DWORD>((exePath.length() + 1) * sizeof(wchar_t)));
            std::wstring exeDir = PathHelper::GetDirectory(exePath);
            RegSetValueExW(hAppPathsKey, L"Path", 0, REG_SZ, reinterpret_cast<const BYTE*>(exeDir.c_str()), static_cast<DWORD>((exeDir.length() + 1) * sizeof(wchar_t)));
            RegCloseKey(hAppPathsKey);
        }

        // 1. HKCU\Software\Classes\Applications\EasyZip.exe
        HKEY hAppKey;
        if (RegCreateKeyExW(HKEY_CURRENT_USER, L"Software\\Classes\\Applications\\EasyZip.exe", 0, NULL, 0, KEY_WRITE, NULL, &hAppKey, NULL) == ERROR_SUCCESS)
        {
            std::wstring appName = L"EasyZip";
            RegSetValueExW(hAppKey, L"FriendlyAppName", 0, REG_SZ, reinterpret_cast<const BYTE*>(appName.c_str()), static_cast<DWORD>((appName.length() + 1) * sizeof(wchar_t)));

            HKEY hIconKey;
            if (RegCreateKeyExW(hAppKey, L"DefaultIcon", 0, NULL, 0, KEY_WRITE, NULL, &hIconKey, NULL) == ERROR_SUCCESS)
            {
                std::wstring iconVal = L"\"" + exePath + L"\",0";
                RegSetValueExW(hIconKey, NULL, 0, REG_SZ, reinterpret_cast<const BYTE*>(iconVal.c_str()), static_cast<DWORD>((iconVal.length() + 1) * sizeof(wchar_t)));
                RegCloseKey(hIconKey);
            }

            HKEY hCmdKey;
            if (RegCreateKeyExW(hAppKey, L"shell\\open\\command", 0, NULL, 0, KEY_WRITE, NULL, &hCmdKey, NULL) == ERROR_SUCCESS)
            {
                std::wstring cmdVal = L"\"" + exePath + L"\" \"%1\"";
                RegSetValueExW(hCmdKey, NULL, 0, REG_SZ, reinterpret_cast<const BYTE*>(cmdVal.c_str()), static_cast<DWORD>((cmdVal.length() + 1) * sizeof(wchar_t)));
                RegCloseKey(hCmdKey);
            }

            HKEY hSupKey;
            if (RegCreateKeyExW(hAppKey, L"SupportedTypes", 0, NULL, 0, KEY_WRITE, NULL, &hSupKey, NULL) == ERROR_SUCCESS)
            {
                for (const auto& ext : FileAssociation::GetSupportedExtensions())
                {
                    RegSetValueExW(hSupKey, ext.c_str(), 0, REG_SZ, reinterpret_cast<const BYTE*>(L""), sizeof(wchar_t));
                }
                RegCloseKey(hSupKey);
            }

            RegCloseKey(hAppKey);
        }

        // 2. HKCU\Software\EasyZip\Capabilities
        HKEY hCapKey;
        if (RegCreateKeyExW(HKEY_CURRENT_USER, L"Software\\EasyZip\\Capabilities", 0, NULL, 0, KEY_WRITE, NULL, &hCapKey, NULL) == ERROR_SUCCESS)
        {
            std::wstring name = L"EasyZip";
            std::wstring desc = L"EasyZip High Performance Archiver";
            RegSetValueExW(hCapKey, L"ApplicationName", 0, REG_SZ, reinterpret_cast<const BYTE*>(name.c_str()), static_cast<DWORD>((name.length() + 1) * sizeof(wchar_t)));
            RegSetValueExW(hCapKey, L"ApplicationDescription", 0, REG_SZ, reinterpret_cast<const BYTE*>(desc.c_str()), static_cast<DWORD>((desc.length() + 1) * sizeof(wchar_t)));

            HKEY hAssocKey;
            if (RegCreateKeyExW(hCapKey, L"FileAssociations", 0, NULL, 0, KEY_WRITE, NULL, &hAssocKey, NULL) == ERROR_SUCCESS)
            {
                std::wstring progId = L"EasyZip.Archive";
                for (const auto& ext : FileAssociation::GetSupportedExtensions())
                {
                    RegSetValueExW(hAssocKey, ext.c_str(), 0, REG_SZ, reinterpret_cast<const BYTE*>(progId.c_str()), static_cast<DWORD>((progId.length() + 1) * sizeof(wchar_t)));
                }
                RegCloseKey(hAssocKey);
            }
            RegCloseKey(hCapKey);
        }

        // 3. HKCU\Software\RegisteredApplications
        HKEY hRegAppKey;
        if (RegCreateKeyExW(HKEY_CURRENT_USER, L"Software\\RegisteredApplications", 0, NULL, 0, KEY_WRITE, NULL, &hRegAppKey, NULL) == ERROR_SUCCESS)
        {
            std::wstring capPath = L"Software\\EasyZip\\Capabilities";
            RegSetValueExW(hRegAppKey, L"EasyZip", 0, REG_SZ, reinterpret_cast<const BYTE*>(capPath.c_str()), static_cast<DWORD>((capPath.length() + 1) * sizeof(wchar_t)));
            RegCloseKey(hRegAppKey);
        }
    }

    bool FileAssociation::IsExtensionRegistered(const std::wstring& ext)
    {
        std::wstring dotExt = ext;
        if (!dotExt.empty() && dotExt[0] != L'.') dotExt = L"." + dotExt;

        // 1. Check Windows 10/11 UserChoice (the actual active handler)
        std::wstring userChoiceKey = L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\FileExts\\" + dotExt + L"\\UserChoice";
        HKEY hUserChoice;
        if (RegOpenKeyExW(HKEY_CURRENT_USER, userChoiceKey.c_str(), 0, KEY_READ, &hUserChoice) == ERROR_SUCCESS)
        {
            wchar_t progVal[128] = { 0 };
            DWORD valSize = sizeof(progVal);
            LONG res = RegQueryValueExW(hUserChoice, L"ProgId", NULL, NULL, reinterpret_cast<LPBYTE>(progVal), &valSize);
            RegCloseKey(hUserChoice);

            if (res == ERROR_SUCCESS)
            {
                std::wstring progId = progVal;
                if (progId.rfind(L"EasyZip.", 0) == 0)
                {
                    return true;
                }
            }
        }

        // 2. Check HKCU\Software\Classes\<.ext> default value
        std::wstring keyPath = L"Software\\Classes\\" + dotExt;
        HKEY hKey;
        if (RegOpenKeyExW(HKEY_CURRENT_USER, keyPath.c_str(), 0, KEY_READ, &hKey) == ERROR_SUCCESS)
        {
            wchar_t val[128] = { 0 };
            DWORD valSize = sizeof(val);
            LONG res = RegQueryValueExW(hKey, NULL, NULL, NULL, reinterpret_cast<LPBYTE>(val), &valSize);
            RegCloseKey(hKey);

            if (res == ERROR_SUCCESS)
            {
                std::wstring progId = val;
                if (progId.rfind(L"EasyZip.", 0) == 0)
                {
                    return true;
                }
            }
        }

        return false;
    }

    bool FileAssociation::RegisterExtension(const std::wstring& ext)
    {
        std::wstring dotExt = ext;
        if (!dotExt.empty() && dotExt[0] != L'.') dotExt = L"." + dotExt;

        std::wstring cleanExt = dotExt.substr(1);
        std::wstring specificProgId = L"EasyZip." + cleanExt;
        std::wstring generalProgId = L"EasyZip.Archive";
        std::wstring exePath = PathHelper::GetExePath();

        // 1. Register both specific and general ProgIDs with complete command line
        RegisterSingleProgId(specificProgId, L"EasyZip " + cleanExt + L" Archive", exePath);
        RegisterSingleProgId(generalProgId, L"EasyZip Archive", exePath);

        // 2. Register Applications and Capabilities for Windows 11
        RegisterApplicationInfo(exePath);

        // 3. HKCU\Software\Classes\<dotExt> -> unified generalProgId (EasyZip.Archive)
        HKEY hExtKey;
        std::wstring extKeyPath = L"Software\\Classes\\" + dotExt;
        if (RegCreateKeyExW(HKEY_CURRENT_USER, extKeyPath.c_str(), 0, NULL, 0, KEY_WRITE, NULL, &hExtKey, NULL) == ERROR_SUCCESS)
        {
            RegSetValueExW(hExtKey, NULL, 0, REG_SZ, reinterpret_cast<const BYTE*>(generalProgId.c_str()), static_cast<DWORD>((generalProgId.length() + 1) * sizeof(wchar_t)));

            // OpenWithProgids
            HKEY hOwpKey;
            if (RegCreateKeyExW(hExtKey, L"OpenWithProgids", 0, NULL, 0, KEY_WRITE, NULL, &hOwpKey, NULL) == ERROR_SUCCESS)
            {
                RegSetValueExW(hOwpKey, generalProgId.c_str(), 0, REG_SZ, reinterpret_cast<const BYTE*>(L""), sizeof(wchar_t));
                RegSetValueExW(hOwpKey, specificProgId.c_str(), 0, REG_SZ, reinterpret_cast<const BYTE*>(L""), sizeof(wchar_t));
                RegCloseKey(hOwpKey);
            }

            RegCloseKey(hExtKey);
        }

        // 4. FileExts\<dotExt>\OpenWithProgids
        std::wstring fileExtsOwp = L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\FileExts\\" + dotExt + L"\\OpenWithProgids";
        HKEY hFeOwp;
        if (RegCreateKeyExW(HKEY_CURRENT_USER, fileExtsOwp.c_str(), 0, NULL, 0, KEY_WRITE, NULL, &hFeOwp, NULL) == ERROR_SUCCESS)
        {
            RegSetValueExW(hFeOwp, generalProgId.c_str(), 0, REG_NONE, NULL, 0);
            RegSetValueExW(hFeOwp, specificProgId.c_str(), 0, REG_NONE, NULL, 0);
            RegCloseKey(hFeOwp);
        }

        return true;
    }

    bool FileAssociation::UnregisterExtension(const std::wstring& ext)
    {
        std::wstring dotExt = ext;
        if (!dotExt.empty() && dotExt[0] != L'.') dotExt = L"." + dotExt;

        std::wstring cleanExt = dotExt.substr(1);
        std::wstring specificProgId = L"EasyZip." + cleanExt;

        // Delete HKCU\Software\Classes\<dotExt> if it points to EasyZip
        if (IsExtensionRegistered(dotExt))
        {
            std::wstring extKeyPath = L"Software\\Classes\\" + dotExt;
            RegDeleteKeyW(HKEY_CURRENT_USER, extKeyPath.c_str());
        }

        std::wstring progKeyPath = L"Software\\Classes\\" + specificProgId;
        RegDeleteTreeW(HKEY_CURRENT_USER, progKeyPath.c_str());

        return true;
    }

    void FileAssociation::RegisterAll()
    {
        for (const auto& ext : GetSupportedExtensions())
        {
            RegisterExtension(ext);
        }
        RegisterShellMenu();
    }

    void FileAssociation::RegisterShellMenu()
    {
        AppSettings::Instance().Save();
        CleanLegacyShellExtension();
        RegisterNewFolderVerb(AppSettings::Instance().GetMenuNewFolder());
        RegisterCompressVerbs(AppSettings::Instance().GetMenuCompress());
        BlockWindowsDefaultZipMenu(true);

        // Update all ProgIDs with current settings (ExtractSmart, ExtractHere, ExtractFolder)
        std::wstring exePath = PathHelper::GetExePath();
        RegisterSingleProgId(L"EasyZip.Archive", L"EasyZip Archive", exePath);
        for (const auto& ext : GetSupportedExtensions())
        {
            std::wstring cleanExt = (ext[0] == L'.') ? ext.substr(1) : ext;
            RegisterSingleProgId(L"EasyZip." + cleanExt, L"EasyZip " + cleanExt + L" Archive", exePath);
        }

        RefreshShellIcons();
    }

    void FileAssociation::UnregisterShellMenu()
    {
        CleanLegacyShellExtension();
        RegisterNewFolderVerb(false);
        RegisterCompressVerbs(false);
        BlockWindowsDefaultZipMenu(false);

        // Delete extract verbs from EasyZip.Archive
        HKEY hProgKey;
        if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Classes\\EasyZip.Archive", 0, KEY_WRITE, &hProgKey) == ERROR_SUCCESS)
        {
            RegDeleteTreeW(hProgKey, L"shell\\ExtractSmart");
            RegDeleteTreeW(hProgKey, L"shell\\ExtractHere");
            RegDeleteTreeW(hProgKey, L"shell\\ExtractFolder");
            RegCloseKey(hProgKey);
        }

        for (const auto& ext : GetSupportedExtensions())
        {
            std::wstring cleanExt = (ext[0] == L'.') ? ext.substr(1) : ext;
            std::wstring subKey = L"Software\\Classes\\EasyZip." + cleanExt;
            if (RegOpenKeyExW(HKEY_CURRENT_USER, subKey.c_str(), 0, KEY_WRITE, &hProgKey) == ERROR_SUCCESS)
            {
                RegDeleteTreeW(hProgKey, L"shell\\ExtractSmart");
                RegDeleteTreeW(hProgKey, L"shell\\ExtractHere");
                RegDeleteTreeW(hProgKey, L"shell\\ExtractFolder");
                RegCloseKey(hProgKey);
            }
        }

        RefreshShellIcons();
    }

    void FileAssociation::RegisterShellExtensionApproved(bool reg)
    {
        // Legacy stub: Clean legacy COM extension instead
        CleanLegacyShellExtension();
    }

    void FileAssociation::BlockWindowsDefaultZipMenu(bool block)
    {
        static const wchar_t* blockedClsids[] = {
            L"{b8cdcb65-b1bf-4b42-9428-1dfdb7ee92af}", // Compressed (zipped) Folder Context Menu (Windows ZIP 기본 '압축 풀기')
            L"{EE07CEF5-3441-4CFB-870A-4002C724783A}"  // Compressed Archive Folder Context Menu (Windows 11 아카이브 '압축 풀기')
        };

        const wchar_t* blockedKeyPath = L"Software\\Microsoft\\Windows\\CurrentVersion\\Shell Extensions\\Blocked";
        const HKEY rootKeys[] = { HKEY_LOCAL_MACHINE, HKEY_CURRENT_USER };

        for (HKEY root : rootKeys)
        {
            if (block)
            {
                HKEY hKey = NULL;
                if (RegCreateKeyExW(root, blockedKeyPath, 0, NULL, 0, KEY_WRITE, NULL, &hKey, NULL) == ERROR_SUCCESS)
                {
                    for (const auto* clsid : blockedClsids)
                    {
                        RegSetValueExW(hKey, clsid, 0, REG_SZ, reinterpret_cast<const BYTE*>(L""), sizeof(wchar_t));
                    }
                    RegCloseKey(hKey);
                }
            }
            else
            {
                HKEY hKey = NULL;
                if (RegOpenKeyExW(root, blockedKeyPath, 0, KEY_WRITE, &hKey) == ERROR_SUCCESS)
                {
                    for (const auto* clsid : blockedClsids)
                    {
                        RegDeleteValueW(hKey, clsid);
                    }
                    RegCloseKey(hKey);
                }
            }
        }
    }

    void FileAssociation::RegisterNewFolderVerb(bool enable)
    {
        static const wchar_t* allRoots[] = {
            L"Software\\Classes\\DesktopBackground\\shell\\EasyZip.NewFolder",
            L"Software\\Classes\\Directory\\Background\\shell\\EasyZip.NewFolder",
            L"Software\\Classes\\Directory\\shell\\EasyZip.NewFolder",
            L"Software\\Classes\\*\\shell\\EasyZip.NewFolder"
        };

        // 기존 키들 먼저 깔끔하게 정리 (DesktopBackground 중복 방지)
        for (const auto* r : allRoots)
        {
            RegDeleteTreeW(HKEY_CURRENT_USER, r);
        }

        if (!enable) return;

        // Directory\Background가 바탕화면과 폴더 빈 공간을 모두 커버하므로 1개만 등록하여 중복 노출 방지
        static const wchar_t* registerRoots[] = {
            L"Software\\Classes\\Directory\\Background\\shell\\EasyZip.NewFolder",
            L"Software\\Classes\\Directory\\shell\\EasyZip.NewFolder",
            L"Software\\Classes\\*\\shell\\EasyZip.NewFolder"
        };

        std::wstring exePath = PathHelper::GetExePath();
        std::wstring iconVal = L"shell32.dll,3"; // 윈도우 공식 표준 노란색 폴더 아이콘

        for (const auto* r : registerRoots)
        {
            HKEY hKey = NULL;
            if (RegCreateKeyExW(HKEY_CURRENT_USER, r, 0, NULL, 0, KEY_WRITE, NULL, &hKey, NULL) == ERROR_SUCCESS)
            {
                // 1. (Default): 새 폴더 만들기(&N)
                const wchar_t text[] = L"새 폴더 만들기(&N)";
                RegSetValueExW(hKey, NULL, 0, REG_SZ, reinterpret_cast<const BYTE*>(text), sizeof(text));

                // 2. Position: Top (★ 마우스 컨텍스트 메뉴의 절대 최상단 고정!)
                const wchar_t pos[] = L"Top";
                RegSetValueExW(hKey, L"Position", 0, REG_SZ, reinterpret_cast<const BYTE*>(pos), sizeof(pos));

                // 3. Icon
                RegSetValueExW(hKey, L"Icon", 0, REG_SZ, reinterpret_cast<const BYTE*>(iconVal.c_str()), static_cast<DWORD>((iconVal.length() + 1) * sizeof(wchar_t)));

                // 4. command: "<exePath>" --newfolder "%V" (또는 "%1")
                HKEY hCmd = NULL;
                if (RegCreateKeyExW(hKey, L"command", 0, NULL, 0, KEY_WRITE, NULL, &hCmd, NULL) == ERROR_SUCCESS)
                {
                    std::wstring cmdVal;
                    if (wcsstr(r, L"\\*\\") != nullptr)
                    {
                        cmdVal = L"\"" + exePath + L"\" --newfolder \"%1\"";
                    }
                    else
                    {
                        cmdVal = L"\"" + exePath + L"\" --newfolder \"%V\"";
                    }
                    RegSetValueExW(hCmd, NULL, 0, REG_SZ, reinterpret_cast<const BYTE*>(cmdVal.c_str()), static_cast<DWORD>((cmdVal.length() + 1) * sizeof(wchar_t)));
                    RegCloseKey(hCmd);
                }

                RegCloseKey(hKey);
            }
        }
    }

    void FileAssociation::UnregisterAll()
    {
        for (const auto& ext : GetSupportedExtensions())
        {
            UnregisterExtension(ext);
        }
        std::wstring generalProgKey = L"Software\\Classes\\EasyZip.Archive";
        RegDeleteTreeW(HKEY_CURRENT_USER, generalProgKey.c_str());
        RegDeleteTreeW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\App Paths\\EasyZip.exe");

        UnregisterShellMenu();
    }

    void FileAssociation::RefreshShellIcons()
    {
        SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, NULL, NULL);
    }

    void FileAssociation::OpenWindowsDefaultAppsSettings()
    {
        // Windows 11 deep-link: opens directly to EasyZip's default apps page
        HINSTANCE hInst = ShellExecuteW(NULL, L"open", L"ms-settings:defaultapps?registeredAppUser=EasyZip", NULL, NULL, SW_SHOWNORMAL);
        if (reinterpret_cast<INT_PTR>(hInst) <= 32)
        {
            ShellExecuteW(NULL, L"open", L"ms-settings:defaultapps", NULL, NULL, SW_SHOWNORMAL);
        }
    }

    void FileAssociation::LaunchAdvancedAssociationUI()
    {
        OpenWindowsDefaultAppsSettings();
    }
}
