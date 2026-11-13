#include "PathHelper.h"
#include <shlwapi.h>
#include <algorithm>
#include <sstream>
#include <iomanip>

#pragma comment(lib, "shlwapi.lib")

namespace EasyZip
{
    std::wstring PathHelper::GetDirectory(const std::wstring& path)
    {
        wchar_t buf[MAX_PATH * 2];
        wcsncpy_s(buf, path.c_str(), _TRUNCATE);
        PathRemoveFileSpecW(buf);
        return buf;
    }

    std::wstring PathHelper::GetFileName(const std::wstring& path)
    {
        const wchar_t* p = PathFindFileNameW(path.c_str());
        return p ? p : L"";
    }

    std::wstring PathHelper::GetFileNameWithoutExtension(const std::wstring& path)
    {
        std::wstring name = GetFileName(path);
        size_t dot = name.find_last_of(L'.');
        if (dot != std::wstring::npos && dot > 0)
        {
            return name.substr(0, dot);
        }
        return name;
    }

    std::wstring PathHelper::GetArchiveBaseName(const std::wstring& path)
    {
        std::wstring name = GetFileName(path);
        static const std::vector<std::wstring> compoundExts = {
            L".tar.gz", L".tar.bz2", L".tar.xz", L".tar.zst",
            L".7z.001", L".zip.001", L".part01.rar", L".part1.rar"
        };

        std::wstring lowerName = name;
        std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), ::towlower);

        for (const auto& ext : compoundExts)
        {
            if (lowerName.length() > ext.length() &&
                lowerName.rfind(ext) == (lowerName.length() - ext.length()))
            {
                return name.substr(0, name.length() - ext.length());
            }
        }

        return GetFileNameWithoutExtension(path);
    }

    std::wstring PathHelper::GetExtension(const std::wstring& path)
    {
        const wchar_t* p = PathFindExtensionW(path.c_str());
        return p ? p : L"";
    }

    std::wstring PathHelper::Combine(const std::wstring& path1, const std::wstring& path2)
    {
        wchar_t buf[MAX_PATH * 2] = { 0 };
        PathCombineW(buf, path1.c_str(), path2.c_str());
        return buf;
    }

    std::wstring PathHelper::Normalize(const std::wstring& path)
    {
        std::wstring res = path;
        std::replace(res.begin(), res.end(), L'/', L'\\');
        return res;
    }

    std::wstring PathHelper::SanitizeRelativePath(const std::wstring& path)
    {
        std::wstring res = Normalize(path);
        
        // Strip leading drive or backslashes
        while (!res.empty() && (res.front() == L'\\' || res.front() == L'/'))
        {
            res.erase(0, 1);
        }
        if (res.length() >= 2 && res[1] == L':')
        {
            res.erase(0, 2);
            while (!res.empty() && (res.front() == L'\\' || res.front() == L'/'))
            {
                res.erase(0, 1);
            }
        }

        // Prevent directory traversal: remove "../" or "..\"
        size_t pos = 0;
        while ((pos = res.find(L"..\\")) != std::wstring::npos)
        {
            res.erase(pos, 3);
        }

        return res;
    }

    bool PathHelper::FileExists(const std::wstring& path)
    {
        DWORD dw = GetFileAttributesW(path.c_str());
        return (dw != INVALID_FILE_ATTRIBUTES && !(dw & FILE_ATTRIBUTE_DIRECTORY));
    }

    bool PathHelper::DirectoryExists(const std::wstring& path)
    {
        DWORD dw = GetFileAttributesW(path.c_str());
        return (dw != INVALID_FILE_ATTRIBUTES && (dw & FILE_ATTRIBUTE_DIRECTORY));
    }

    bool PathHelper::EnsureDirectoryExists(const std::wstring& path)
    {
        if (DirectoryExists(path)) return true;

        wchar_t folder[MAX_PATH * 2];
        wcsncpy_s(folder, path.c_str(), _TRUNCATE);
        
        for (wchar_t* p = folder; *p; ++p)
        {
            if (*p == L'\\' || *p == L'/')
            {
                wchar_t old = *p;
                *p = L'\0';
                if (wcslen(folder) > 2 && folder[wcslen(folder) - 1] != L':')
                {
                    CreateDirectoryW(folder, NULL);
                }
                *p = old;
            }
        }
        return CreateDirectoryW(folder, NULL) || GetLastError() == ERROR_ALREADY_EXISTS;
    }

    std::wstring PathHelper::GetUniqueDirectoryPath(const std::wstring& parentDir, const std::wstring& baseFolderName)
    {
        std::wstring target = Combine(parentDir, baseFolderName);
        if (!DirectoryExists(target) && !FileExists(target))
        {
            return target;
        }

        int count = 2;
        while (true)
        {
            std::wstring candidate = Combine(parentDir, baseFolderName + L" (" + std::to_wstring(count) + L")");
            if (!DirectoryExists(candidate) && !FileExists(candidate))
            {
                return candidate;
            }
            count++;
        }
    }

    std::wstring PathHelper::GetUniqueFilePath(const std::wstring& parentDir, const std::wstring& baseName, const std::wstring& ext)
    {
        std::wstring dotExt = ext;
        if (!dotExt.empty() && dotExt[0] != L'.')
        {
            dotExt = L"." + dotExt;
        }

        std::wstring target = Combine(parentDir, baseName + dotExt);
        if (!FileExists(target) && !DirectoryExists(target))
        {
            return target;
        }

        int count = 2;
        while (true)
        {
            std::wstring candidate = Combine(parentDir, baseName + L" (" + std::to_wstring(count) + L")" + dotExt);
            if (!FileExists(candidate) && !DirectoryExists(candidate))
            {
                return candidate;
            }
            count++;
        }
    }

    std::wstring PathHelper::GetExePath()
    {
        wchar_t buf[MAX_PATH * 2] = { 0 };
        GetModuleFileNameW(NULL, buf, _countof(buf));
        return buf;
    }

    std::wstring PathHelper::GetExeDirectory()
    {
        return GetDirectory(GetExePath());
    }

    std::wstring PathHelper::FormatFileSize(uint64_t bytes)
    {
        const wchar_t* units[] = { L"B", L"KB", L"MB", L"GB", L"TB" };
        int idx = 0;
        double size = static_cast<double>(bytes);

        while (size >= 1024.0 && idx < 4)
        {
            size /= 1024.0;
            idx++;
        }

        wchar_t buf[64];
        if (idx == 0)
        {
            swprintf_s(buf, L"%I64u B", bytes);
        }
        else
        {
            swprintf_s(buf, L"%.2f %s", size, units[idx]);
        }
        return buf;
    }

    std::wstring PathHelper::FormatSpeed(double bytesPerSec)
    {
        if (bytesPerSec < 0.0) bytesPerSec = 0.0;
        return FormatFileSize(static_cast<uint64_t>(bytesPerSec)) + L"/s";
    }

    std::wstring PathHelper::FormatTime(double seconds)
    {
        if (seconds < 0.0 || seconds > 86400.0 * 30.0) return L"--:--";
        int totalSec = static_cast<int>(seconds);
        int h = totalSec / 3600;
        int m = (totalSec % 3600) / 60;
        int s = totalSec % 60;

        wchar_t buf[32];
        if (h > 0)
        {
            swprintf_s(buf, L"%02d:%02d:%02d", h, m, s);
        }
        else
        {
            swprintf_s(buf, L"%02d:%02d", m, s);
        }
        return buf;
    }

    std::wstring PathHelper::Utf8ToWide(const std::string& str)
    {
        if (str.empty()) return L"";
        int len = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, NULL, 0);
        if (len <= 0) return L"";
        std::vector<wchar_t> buf(len);
        MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, buf.data(), len);
        return std::wstring(buf.data());
    }

    std::string PathHelper::WideToUtf8(const std::wstring& wstr)
    {
        if (wstr.empty()) return "";
        int len = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, NULL, 0, NULL, NULL);
        if (len <= 0) return "";
        std::vector<char> buf(len);
        WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, buf.data(), len, NULL, NULL);
        return std::string(buf.data());
    }

    std::wstring PathHelper::AnsiToWide(const std::string& str, UINT codePage)
    {
        if (str.empty()) return L"";
        int len = MultiByteToWideChar(codePage, 0, str.c_str(), -1, NULL, 0);
        if (len <= 0) return L"";
        std::vector<wchar_t> buf(len);
        MultiByteToWideChar(codePage, 0, str.c_str(), -1, buf.data(), len);
        return std::wstring(buf.data());
    }
}

