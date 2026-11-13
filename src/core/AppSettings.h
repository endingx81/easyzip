#pragma once

#include <windows.h>
#include <string>

namespace EasyZip
{
    class AppSettings
    {
    public:
        static AppSettings& Instance();

        void Load();
        void Save();

        bool GetOpenFolderAfterExtract() const { return m_openFolderAfterExtract; }
        void SetOpenFolderAfterExtract(bool value) { m_openFolderAfterExtract = value; Save(); }

        const std::wstring& GetDefaultFormat() const { return m_defaultFormat; }
        void SetDefaultFormat(const std::wstring& fmt) { m_defaultFormat = fmt; Save(); }

        int GetDefaultLevel() const { return m_defaultLevel; }
        void SetDefaultLevel(int level) { m_defaultLevel = level; Save(); }

        int GetDefaultThreads() const { return m_defaultThreads; }
        void SetDefaultThreads(int threads) { m_defaultThreads = threads; Save(); }

        bool GetEnableShellMenu() const { return m_enableShellMenu; }
        void SetEnableShellMenu(bool value) { m_enableShellMenu = value; Save(); }

        bool GetShowSubmenu() const { return m_showSubmenu; }
        void SetShowSubmenu(bool value) { m_showSubmenu = value; Save(); }

        bool GetMenuExtractHere() const { return m_menuExtractHere; }
        void SetMenuExtractHere(bool value) { m_menuExtractHere = value; Save(); }

        bool GetMenuExtractSmart() const { return m_menuExtractSmart; }
        void SetMenuExtractSmart(bool value) { m_menuExtractSmart = value; Save(); }

        bool GetMenuExtractFolder() const { return m_menuExtractFolder; }
        void SetMenuExtractFolder(bool value) { m_menuExtractFolder = value; Save(); }

        bool GetMenuExtractEach() const { return m_menuExtractEach; }
        void SetMenuExtractEach(bool value) { m_menuExtractEach = value; Save(); }

        bool GetMenuFastZip() const { return m_menuFastZip; }
        void SetMenuFastZip(bool value) { m_menuFastZip = value; Save(); }

        bool GetMenuCompress() const { return m_menuCompress; }
        void SetMenuCompress(bool value) { m_menuCompress = value; Save(); }

        bool GetMenuNewFolder() const { return m_menuNewFolder; }
        void SetMenuNewFolder(bool value) { m_menuNewFolder = value; Save(); }

        const std::wstring& GetLastExtractDir() const { return m_lastExtractDir; }
        void SetLastExtractDir(const std::wstring& dir) { m_lastExtractDir = dir; Save(); }

        bool GetFirstRunDone() const { return m_firstRunDone; }
        void SetFirstRunDone(bool value) { m_firstRunDone = value; Save(); }

    private:
        AppSettings();
        ~AppSettings() = default;

        std::wstring GetIniFilePath();

        bool m_firstRunDone = false;
        bool m_openFolderAfterExtract = false;
        std::wstring m_defaultFormat = L"zip";
        int m_defaultLevel = 5;
        int m_defaultThreads = 0;
        bool m_enableShellMenu = true;
        bool m_showSubmenu = false;
        bool m_menuExtractHere = false;
        bool m_menuExtractSmart = true;
        bool m_menuExtractFolder = true;
        bool m_menuExtractEach = true;
        bool m_menuFastZip = true;
        bool m_menuCompress = true;
        bool m_menuNewFolder = true;
        std::wstring m_lastExtractDir = L"";
    };
}

