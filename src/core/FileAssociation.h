#pragma once

#include <windows.h>
#include <string>
#include <vector>

namespace EasyZip
{
    class FileAssociation
    {
    public:
        static const std::vector<std::wstring>& GetSupportedExtensions();
        static bool IsExtensionRegistered(const std::wstring& ext);
        static bool RegisterExtension(const std::wstring& ext);
        static bool UnregisterExtension(const std::wstring& ext);
        static void RegisterAll();
        static void UnregisterAll();
        static void RegisterShellMenu();
        static void UnregisterShellMenu();
        static void RegisterNewFolderVerb(bool enable);
        static void BlockWindowsDefaultZipMenu(bool block);
        static void RegisterShellExtensionApproved(bool reg);
        static void RefreshShellIcons();

        // Windows 11 & Modern Windows Default Apps integration
        static void OpenWindowsDefaultAppsSettings();
        static void LaunchAdvancedAssociationUI();
    };
}
