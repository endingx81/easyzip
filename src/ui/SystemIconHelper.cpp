#include "SystemIconHelper.h"
#include "PathHelper.h"
#include <shellapi.h>
#include <algorithm>

namespace EasyZip
{
    SystemIconHelper& SystemIconHelper::Instance()
    {
        static SystemIconHelper s_inst;
        return s_inst;
    }

    SystemIconHelper::SystemIconHelper()
    {
        SHFILEINFOW sfi = { 0 };
        m_hImageListSmall = reinterpret_cast<HIMAGELIST>(
            SHGetFileInfoW(L"folder", FILE_ATTRIBUTE_DIRECTORY, &sfi, sizeof(sfi),
                SHGFI_SYSICONINDEX | SHGFI_SMALLICON | SHGFI_USEFILEATTRIBUTES));
        m_folderIconIndex = sfi.iIcon;

        SHFILEINFOW sfiOpen = { 0 };
        SHGetFileInfoW(L"folder", FILE_ATTRIBUTE_DIRECTORY, &sfiOpen, sizeof(sfiOpen),
            SHGFI_SYSICONINDEX | SHGFI_SMALLICON | SHGFI_OPENICON | SHGFI_USEFILEATTRIBUTES);
        m_folderOpenIconIndex = sfiOpen.iIcon;

        // Windows 공식 압축파일(지퍼 폴더) 아이콘 로드 (zipfldr.dll)
        HICON hZipIcon = NULL;
        if (ExtractIconExW(L"C:\\Windows\\System32\\zipfldr.dll", 0, NULL, &hZipIcon, 1) > 0 && hZipIcon)
        {
            if (m_hImageListSmall)
            {
                m_zipIconIndex = ImageList_AddIcon(m_hImageListSmall, hZipIcon);
            }
            DestroyIcon(hZipIcon);
        }

        if (m_zipIconIndex < 0)
        {
            SHFILEINFOW sfiZip = { 0 };
            SHGetFileInfoW(L"dummy.zip", FILE_ATTRIBUTE_NORMAL, &sfiZip, sizeof(sfiZip),
                SHGFI_SYSICONINDEX | SHGFI_SMALLICON | SHGFI_USEFILEATTRIBUTES);
            m_zipIconIndex = sfiZip.iIcon;
        }
    }

    HIMAGELIST SystemIconHelper::GetSystemImageListSmall()
    {
        return m_hImageListSmall;
    }

    int SystemIconHelper::GetFolderIconIndex(bool isOpen)
    {
        return isOpen ? m_folderOpenIconIndex : m_folderIconIndex;
    }

    int SystemIconHelper::GetArchiveIconIndex(const std::wstring& path)
    {
        if (path.empty())
        {
            return m_zipIconIndex >= 0 ? m_zipIconIndex : m_folderIconIndex;
        }

        std::wstring ext = PathHelper::GetExtension(path);
        std::transform(ext.begin(), ext.end(), ext.begin(), ::towlower);

        if (ext == L".zip" && m_zipIconIndex >= 0)
        {
            return m_zipIconIndex;
        }

        int iconIdx = GetIconIndexForPath(path, false);
        if (iconIdx >= 0)
        {
            return iconIdx;
        }

        return m_zipIconIndex >= 0 ? m_zipIconIndex : m_folderIconIndex;
    }

    int SystemIconHelper::GetIconIndexForPath(const std::wstring& path, bool isDirectory)
    {
        if (isDirectory)
        {
            return m_folderIconIndex;
        }

        std::wstring ext = PathHelper::GetExtension(path);
        std::transform(ext.begin(), ext.end(), ext.begin(), ::towlower);

        auto it = m_extensionIconCache.find(ext);
        if (it != m_extensionIconCache.end())
        {
            return it->second;
        }

        SHFILEINFOW sfi = { 0 };
        std::wstring dummyFile = ext.empty() ? L"dummy" : (L"dummy" + ext);
        SHGetFileInfoW(dummyFile.c_str(), FILE_ATTRIBUTE_NORMAL, &sfi, sizeof(sfi),
            SHGFI_SYSICONINDEX | SHGFI_SMALLICON | SHGFI_USEFILEATTRIBUTES);

        int iconIdx = sfi.iIcon;
        m_extensionIconCache[ext] = iconIdx;
        return iconIdx;
    }
}

