#pragma once

#include <windows.h>
#include <string>
#include <vector>
#include <cstdint>

namespace EasyZip
{
    class PathHelper
    {
    public:
        static std::wstring GetDirectory(const std::wstring& path);
        static std::wstring GetFileName(const std::wstring& path);
        static std::wstring GetFileNameWithoutExtension(const std::wstring& path);
        static std::wstring GetArchiveBaseName(const std::wstring& path);
        static std::wstring GetExtension(const std::wstring& path);
        
        static std::wstring Combine(const std::wstring& path1, const std::wstring& path2);
        static std::wstring Normalize(const std::wstring& path);
        static std::wstring SanitizeRelativePath(const std::wstring& path);
        
        static bool FileExists(const std::wstring& path);
        static bool DirectoryExists(const std::wstring& path);
        static bool EnsureDirectoryExists(const std::wstring& path);
        
        static std::wstring GetUniqueDirectoryPath(const std::wstring& parentDir, const std::wstring& baseFolderName);
        static std::wstring GetUniqueFilePath(const std::wstring& parentDir, const std::wstring& baseName, const std::wstring& ext);
        
        static std::wstring GetExePath();
        static std::wstring GetExeDirectory();
        
        static std::wstring FormatFileSize(uint64_t bytes);
        static std::wstring FormatSpeed(double bytesPerSec);
        static std::wstring FormatTime(double seconds);
        
        static std::wstring Utf8ToWide(const std::string& str);
        static std::string WideToUtf8(const std::wstring& wstr);
        static std::wstring AnsiToWide(const std::string& str, UINT codePage = CP_ACP);
    };
}

