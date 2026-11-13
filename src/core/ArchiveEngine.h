#pragma once

#include <windows.h>
#include <string>
#include <vector>
#include <functional>
#include <cstdint>

namespace EasyZip
{
    struct ArchiveEntry
    {
        std::wstring name;              // File/folder display name (e.g. "document.pdf")
        std::wstring fullPath;          // Normalized relative path in archive (e.g. "work/document.pdf")
        bool isDirectory = false;
        uint64_t uncompressedSize = 0;
        uint64_t compressedSize = 0;
        FILETIME lastModified = { 0, 0 };
        std::wstring lastModifiedStr;
        std::wstring crc32Str;
    };

    struct ProgressInfo
    {
        std::wstring currentFile;
        uint64_t processedBytes = 0;
        uint64_t totalBytes = 0;
        uint32_t processedFiles = 0;
        uint32_t totalFiles = 0;
        double speedBytesPerSec = 0.0;
        double remainingSeconds = 0.0;
        std::wstring statusMessage;
    };

    using ProgressCallback = std::function<void(const ProgressInfo& info)>;
    using CancelCallback = std::function<bool()>;

    class ArchiveEngine
    {
    public:
        static bool IsSupportedArchive(const std::wstring& path);

        // Checks if an archive contains only a single root folder (for Smart Extract)
        static bool IsSingleFolderArchive(
            const std::wstring& archivePath,
            std::wstring& outFolderName);

        // Reads metadata of all entries in the archive
        static bool ReadEntries(
            const std::wstring& archivePath,
            std::vector<ArchiveEntry>& outEntries,
            std::wstring& outErrorMessage,
            const std::string& codepage = "");

        // Extracts all files to destDir
        static bool ExtractAll(
            const std::wstring& archivePath,
            const std::wstring& destDir,
            ProgressCallback onProgress = nullptr,
            CancelCallback isCanceled = nullptr,
            std::wstring* outErrorMessage = nullptr,
            const std::string& codepage = "");

        // Extracts only specified files/folders to destDir
        static bool ExtractSelected(
            const std::wstring& archivePath,
            const std::wstring& destDir,
            const std::vector<std::wstring>& targetPaths,
            ProgressCallback onProgress = nullptr,
            CancelCallback isCanceled = nullptr,
            std::wstring* outErrorMessage = nullptr,
            const std::string& codepage = "");

        // Compresses multiple source files/folders into destArchive
        static bool Compress(
            const std::vector<std::wstring>& sourcePaths,
            const std::wstring& destArchive,
            const std::wstring& format = L"zip",
            int level = 5,
            ProgressCallback onProgress = nullptr,
            CancelCallback isCanceled = nullptr,
            std::wstring* outErrorMessage = nullptr);

        // Tests archive integrity
        static bool TestArchive(
            const std::wstring& archivePath,
            ProgressCallback onProgress = nullptr,
            CancelCallback isCanceled = nullptr,
            std::wstring* outErrorMessage = nullptr,
            const std::string& codepage = "");
    };
}

