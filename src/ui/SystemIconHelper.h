#pragma once

#include <windows.h>
#include <commctrl.h>
#include <string>
#include <unordered_map>

namespace EasyZip
{
    class SystemIconHelper
    {
    public:
        static SystemIconHelper& Instance();

        HIMAGELIST GetSystemImageListSmall();
        int GetIconIndexForPath(const std::wstring& path, bool isDirectory);
        int GetFolderIconIndex(bool isOpen = false);
        int GetArchiveIconIndex(const std::wstring& path = L"");

    private:
        SystemIconHelper();
        ~SystemIconHelper() = default;

        HIMAGELIST m_hImageListSmall = NULL;
        std::unordered_map<std::wstring, int> m_extensionIconCache;
        int m_folderIconIndex = -1;
        int m_folderOpenIconIndex = -1;
        int m_zipIconIndex = -1;
    };
}

