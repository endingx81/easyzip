#include "ArchiveEngine.h"
#include "PathHelper.h"
#include "EncodingHelper.h"
#include <archive.h>
#include <archive_entry.h>
#include <chrono>
#include <unordered_set>
#include <algorithm>

namespace EasyZip
{
    static FILETIME UnixTimeToFILETIME(time_t t)
    {
        LONGLONG ll = Int32x32To64(static_cast<DWORD>(t), 10000000) + 116444736000000000;
        FILETIME ft;
        ft.dwLowDateTime = static_cast<DWORD>(ll);
        ft.dwHighDateTime = static_cast<DWORD>(ll >> 32);
        return ft;
    }

    static std::wstring FormatFileTime(const FILETIME& ft)
    {
        if (ft.dwLowDateTime == 0 && ft.dwHighDateTime == 0) return L"";
        SYSTEMTIME st;
        if (!FileTimeToSystemTime(&ft, &st)) return L"";
        SYSTEMTIME local;
        if (!SystemTimeToTzSpecificLocalTime(NULL, &st, &local)) return L"";

        wchar_t buf[64];
        swprintf_s(buf, L"%04d-%02d-%02d %02d:%02d:%02d",
            local.wYear, local.wMonth, local.wDay,
            local.wHour, local.wMinute, local.wSecond);
        return buf;
    }

    static std::string ResolveCodepage(const std::string& codepage)
    {
        if (!codepage.empty()) return codepage;
        return EncodingHelper::GetDefaultCodepageName();
    }

    static void ConfigureReaderCharset(struct archive* a, const std::string& codepage)
    {
        std::string cp = ResolveCodepage(codepage);
        // Apply hdrcharset globally
        std::string opt = "hdrcharset=" + cp;
        archive_read_set_options(a, opt.c_str());
        // Specifically apply to modules known for legacy encodings
        archive_read_set_format_option(a, "zip", "hdrcharset", cp.c_str());
        archive_read_set_format_option(a, "tar", "hdrcharset", cp.c_str());
        archive_read_set_format_option(a, "cpio", "hdrcharset", cp.c_str());
    }

    static std::wstring GetEntryPath(struct archive_entry* entry, UINT codepageId = 949)
    {
        const wchar_t* wpath = archive_entry_pathname_w(entry);
        if (wpath && wcslen(wpath) > 0)
        {
            return PathHelper::Normalize(wpath);
        }

        const char* u8path = archive_entry_pathname_utf8(entry);
        if (u8path && strlen(u8path) > 0)
        {
            return PathHelper::Normalize(PathHelper::Utf8ToWide(u8path));
        }

        const char* mpath = archive_entry_pathname(entry);
        if (mpath && strlen(mpath) > 0)
        {
            return PathHelper::Normalize(EncodingHelper::DecodeSmart(mpath, strlen(mpath), codepageId));
        }

        return L"";
    }

    bool ArchiveEngine::IsSupportedArchive(const std::wstring& path)
    {
        std::wstring ext = PathHelper::GetExtension(path);
        std::transform(ext.begin(), ext.end(), ext.begin(), ::towlower);

        static const std::unordered_set<std::wstring> s_exts = {
            L".zip", L".7z", L".rar", L".tar", L".gz", L".tgz",
            L".bz2", L".tbz2", L".xz", L".txz", L".iso", L".cab",
            L".alz", L".egg", L".001"
        };
        if (s_exts.find(ext) != s_exts.end()) return true;

        if (ext.length() == 4 && (ext[1] == L'r' || ext[1] == L'z') &&
            iswdigit(ext[2]) && iswdigit(ext[3]))
        {
            return true;
        }

        return false;
    }

    int ArchiveEngine::OpenArchiveRead(struct archive* a, const std::wstring& archivePath, size_t bufferSize)
    {
        std::vector<std::wstring> volumes = PathHelper::GetVolumeFiles(archivePath);
        if (volumes.size() <= 1)
        {
            return archive_read_open_filename_w(a, archivePath.c_str(), bufferSize);
        }

        std::vector<const wchar_t*> volPtrs;
        volPtrs.reserve(volumes.size() + 1);
        for (const auto& v : volumes)
        {
            volPtrs.push_back(v.c_str());
        }
        volPtrs.push_back(nullptr);

        return archive_read_open_filenames_w(a, volPtrs.data(), bufferSize);
    }

    bool ArchiveEngine::IsSingleFolderArchive(const std::wstring& archivePath, std::wstring& outFolderName)
    {
        outFolderName.clear();

        struct archive* a = archive_read_new();
        if (!a) return false;

        archive_read_support_filter_all(a);
        archive_read_support_format_all(a);

        ConfigureReaderCharset(a, "");

        int r = OpenArchiveRead(a, archivePath, 1024 * 512);
        if (r != ARCHIVE_OK)
        {
            archive_read_free(a);
            return false;
        }

        std::unordered_set<std::wstring> topLevelRoots;
        bool hasRootFiles = false;
        int entryCount = 0;

        struct archive_entry* entry = nullptr;
        while (archive_read_next_header(a, &entry) == ARCHIVE_OK)
        {
            std::wstring rawPath = GetEntryPath(entry, CP_ACP);
            if (rawPath.empty()) continue;

            entryCount++;

            std::replace(rawPath.begin(), rawPath.end(), L'\\', L'/');

            size_t start = rawPath.find_first_not_of(L'/');
            if (start == std::wstring::npos) continue;
            std::wstring normPath = rawPath.substr(start);

            while (!normPath.empty() && normPath.back() == L'/')
            {
                normPath.pop_back();
            }
            if (normPath.empty()) continue;

            size_t slashPos = normPath.find(L'/');
            if (slashPos == std::wstring::npos)
            {
                bool isDir = (archive_entry_filetype(entry) == AE_IFDIR);
                if (!isDir && (rawPath.back() == L'/' || rawPath.back() == L'\\'))
                {
                    isDir = true;
                }

                if (isDir)
                {
                    topLevelRoots.insert(normPath);
                }
                else
                {
                    hasRootFiles = true;
                }
            }
            else
            {
                std::wstring rootName = normPath.substr(0, slashPos);
                topLevelRoots.insert(rootName);
            }

            if (hasRootFiles || topLevelRoots.size() > 1)
            {
                break;
            }
        }

        archive_read_free(a);

        if (entryCount > 0 && !hasRootFiles && topLevelRoots.size() == 1)
        {
            outFolderName = *topLevelRoots.begin();
            return true;
        }

        return false;
    }

    bool ArchiveEngine::ReadEntries(
        const std::wstring& archivePath,
        std::vector<ArchiveEntry>& outEntries,
        std::wstring& outErrorMessage,
        const std::string& codepage)
    {
        outEntries.clear();
        outErrorMessage.clear();

        struct archive* a = archive_read_new();
        archive_read_support_filter_all(a);
        archive_read_support_format_all(a);

        ConfigureReaderCharset(a, codepage);

        int r = OpenArchiveRead(a, archivePath, 1024 * 512);
        if (r != ARCHIVE_OK)
        {
            const char* err = archive_error_string(a);
            outErrorMessage = err ? PathHelper::Utf8ToWide(err) : L"아카이브를 열 수 없습니다.";
            archive_read_free(a);
            return false;
        }

        UINT cpId = EncodingHelper::NameToCodepage(ResolveCodepage(codepage));
        struct archive_entry* entry = nullptr;
        while (archive_read_next_header(a, &entry) == ARCHIVE_OK)
        {
            std::wstring rawPath = GetEntryPath(entry, cpId);
            if (rawPath.empty()) continue;

            bool isDir = (archive_entry_filetype(entry) == AE_IFDIR);
            if (!isDir && (rawPath.back() == L'\\' || rawPath.back() == L'/'))
            {
                isDir = true;
            }

            std::wstring cleanPath = rawPath;
            while (!cleanPath.empty() && (cleanPath.back() == L'\\' || cleanPath.back() == L'/'))
            {
                cleanPath.pop_back();
            }

            std::wstring name = PathHelper::GetFileName(cleanPath);
            if (name.empty()) name = cleanPath;

            ArchiveEntry item;
            item.name = name;
            item.fullPath = cleanPath;
            item.isDirectory = isDir;
            item.uncompressedSize = isDir ? 0 : archive_entry_size(entry);
            item.compressedSize = 0;

            time_t mtime = archive_entry_mtime(entry);
            if (mtime > 0)
            {
                item.lastModified = UnixTimeToFILETIME(mtime);
                item.lastModifiedStr = FormatFileTime(item.lastModified);
            }

            outEntries.push_back(item);
        }

        archive_read_free(a);
        return true;
    }

    bool ArchiveEngine::ExtractAll(
        const std::wstring& archivePath,
        const std::wstring& destDir,
        ProgressCallback onProgress,
        CancelCallback isCanceled,
        std::wstring* outErrorMessage,
        const std::string& codepage)
    {
        std::vector<std::wstring> emptyTargets;
        return ExtractSelected(archivePath, destDir, emptyTargets, onProgress, isCanceled, outErrorMessage, codepage);
    }

    bool ArchiveEngine::ExtractSelected(
        const std::wstring& archivePath,
        const std::wstring& destDir,
        const std::vector<std::wstring>& targetPaths,
        ProgressCallback onProgress,
        CancelCallback isCanceled,
        std::wstring* outErrorMessage,
        const std::string& codepage)
    {
        PathHelper::EnsureDirectoryExists(destDir);

        std::unordered_set<std::wstring> targetSet;
        for (const auto& t : targetPaths)
        {
            targetSet.insert(PathHelper::Normalize(t));
        }
        bool hasFilter = !targetSet.empty();

        UINT cpId = EncodingHelper::NameToCodepage(ResolveCodepage(codepage));

        // 1. First pass to calculate total bytes and file count
        uint64_t totalBytes = 0;
        uint32_t totalFiles = 0;
        {
            struct archive* aScan = archive_read_new();
            archive_read_support_filter_all(aScan);
            archive_read_support_format_all(aScan);
            ConfigureReaderCharset(aScan, codepage);

            if (OpenArchiveRead(aScan, archivePath, 1024 * 512) == ARCHIVE_OK)
            {
                struct archive_entry* entry = nullptr;
                while (archive_read_next_header(aScan, &entry) == ARCHIVE_OK)
                {
                    std::wstring p = GetEntryPath(entry, cpId);
                    if (p.empty()) continue;

                    bool isDir = (archive_entry_filetype(entry) == AE_IFDIR) || (p.back() == L'\\');
                    if (hasFilter)
                    {
                        std::wstring norm = PathHelper::Normalize(p);
                        while (!norm.empty() && norm.back() == L'\\') norm.pop_back();

                        bool match = false;
                        for (const auto& target : targetSet)
                        {
                            if (norm == target || norm.rfind(target + L"\\", 0) == 0)
                            {
                                match = true;
                                break;
                            }
                        }
                        if (!match) continue;
                    }

                    if (!isDir)
                    {
                        totalBytes += archive_entry_size(entry);
                        totalFiles++;
                    }
                }
            }
            archive_read_free(aScan);
        }

        // 2. Perform actual extraction
        struct archive* a = archive_read_new();
        archive_read_support_filter_all(a);
        archive_read_support_format_all(a);
        ConfigureReaderCharset(a, codepage);

        if (OpenArchiveRead(a, archivePath, 1024 * 1024) != ARCHIVE_OK)
        {
            if (outErrorMessage)
            {
                const char* err = archive_error_string(a);
                *outErrorMessage = err ? PathHelper::Utf8ToWide(err) : L"아카이브를 열 수 없습니다.";
            }
            archive_read_free(a);
            return false;
        }

        uint64_t processedBytes = 0;
        uint32_t processedFiles = 0;
        auto startTime = std::chrono::steady_clock::now();
        auto lastProgressUpdate = startTime;
        uint64_t nextProgressBytes = 2 * 1024 * 1024;

        bool hasExtractionError = false;
        std::vector<std::wstring> errorList;

        struct archive_entry* entry = nullptr;
        while (archive_read_next_header(a, &entry) == ARCHIVE_OK)
        {
            if (isCanceled && isCanceled())
            {
                archive_read_free(a);
                if (outErrorMessage) *outErrorMessage = L"사용자에 의해 작업이 취소되었습니다.";
                return false;
            }

            std::wstring rawPath = GetEntryPath(entry, cpId);
            if (rawPath.empty()) continue;

            bool isDir = (archive_entry_filetype(entry) == AE_IFDIR) || (rawPath.back() == L'\\');
            std::wstring safeRelative = PathHelper::SanitizeRelativePath(rawPath);
            if (safeRelative.empty()) continue;

            if (hasFilter)
            {
                std::wstring norm = safeRelative;
                while (!norm.empty() && norm.back() == L'\\') norm.pop_back();

                bool match = false;
                for (const auto& target : targetSet)
                {
                    if (norm == target || norm.rfind(target + L"\\", 0) == 0)
                    {
                        match = true;
                        break;
                    }
                }
                if (!match) continue;
            }

            std::wstring targetFullPath = PathHelper::Combine(destDir, safeRelative);

            if (isDir)
            {
                PathHelper::EnsureDirectoryExists(targetFullPath);
                continue;
            }

            PathHelper::EnsureDirectoryExists(PathHelper::GetDirectory(targetFullPath));

            HANDLE hOut = CreateFileW(
                targetFullPath.c_str(),
                GENERIC_WRITE,
                0,
                NULL,
                CREATE_ALWAYS,
                FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN,
                NULL);

            if (hOut == INVALID_HANDLE_VALUE)
            {
                continue;
            }

            const void* buff = nullptr;
            size_t size = 0;
            la_int64_t offset = 0;

            int blockRet = ARCHIVE_OK;
            la_int64_t declaredSize = archive_entry_size(entry);
            bool hasDeclaredSize = archive_entry_size_is_set(entry) != 0;
            uint64_t entryWritten = 0;

            while ((blockRet = archive_read_data_block(a, &buff, &size, &offset)) == ARCHIVE_OK)
            {
                if (isCanceled && isCanceled())
                {
                    CloseHandle(hOut);
                    archive_read_free(a);
                    if (outErrorMessage) *outErrorMessage = L"사용자에 의해 작업이 취소되었습니다.";
                    return false;
                }

                DWORD written = 0;
                WriteFile(hOut, buff, static_cast<DWORD>(size), &written, NULL);
                entryWritten += written;
                processedBytes += written;

                if (onProgress)
                {
                    auto now = std::chrono::steady_clock::now();
                    auto msElapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastProgressUpdate).count();
                    if (msElapsed >= 60 || processedBytes >= nextProgressBytes)
                    {
                        lastProgressUpdate = now;
                        nextProgressBytes = processedBytes + (2 * 1024 * 1024);

                        double elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - startTime).count() / 1000.0;
                        double speed = (elapsed > 0.05) ? (processedBytes / elapsed) : 0.0;
                        double remaining = (speed > 0.0 && totalBytes > processedBytes) ? ((totalBytes - processedBytes) / speed) : 0.0;

                        ProgressInfo info;
                        info.currentFile = PathHelper::GetFileName(safeRelative);
                        info.processedBytes = processedBytes;
                        info.totalBytes = totalBytes > 0 ? totalBytes : processedBytes;
                        info.processedFiles = processedFiles;
                        info.totalFiles = totalFiles > 0 ? totalFiles : processedFiles;
                        info.speedBytesPerSec = speed;
                        info.remainingSeconds = remaining;
                        info.statusMessage = L"압축 푸는 중: " + info.currentFile;
                        onProgress(info);
                    }
                }
            }

            CloseHandle(hOut);

            if (blockRet != ARCHIVE_OK && blockRet != ARCHIVE_EOF)
            {
                hasExtractionError = true;
                const char* err = archive_error_string(a);
                std::wstring errStr = err ? PathHelper::Utf8ToWide(err) : L"데이터 블록 읽기 실패";
                errorList.push_back(safeRelative + L": " + errStr);
            }
            else if (hasDeclaredSize && declaredSize > 0 && entryWritten < static_cast<uint64_t>(declaredSize))
            {
                hasExtractionError = true;
                std::wstring errStr = L"아카이브가 손상되었거나 끝부분이 잘려 있습니다 (선언: " + 
                    PathHelper::FormatFileSize(declaredSize) + L", 실제: " + 
                    PathHelper::FormatFileSize(entryWritten) + L")";
                errorList.push_back(safeRelative + L": " + errStr);
            }

            processedFiles++;
        }

        archive_read_free(a);

        if (onProgress)
        {
            ProgressInfo doneInfo;
            doneInfo.currentFile = hasExtractionError ? L"오류 발생" : L"완료";
            doneInfo.processedBytes = processedBytes;
            doneInfo.totalBytes = totalBytes > 0 ? totalBytes : processedBytes;
            doneInfo.processedFiles = processedFiles;
            doneInfo.totalFiles = totalFiles > 0 ? totalFiles : processedFiles;
            doneInfo.speedBytesPerSec = 0.0;
            doneInfo.remainingSeconds = 0.0;
            doneInfo.statusMessage = hasExtractionError ? L"일부 파일 압축 풀기 실패" : L"압축 풀기 완료";
            onProgress(doneInfo);
        }

        if (hasExtractionError)
        {
            if (outErrorMessage)
            {
                std::wstring combined;
                for (size_t i = 0; i < errorList.size(); ++i)
                {
                    if (i > 0) combined += L"\n";
                    combined += errorList[i];
                    if (i >= 2 && errorList.size() > 3)
                    {
                        combined += L"\n... 외 " + std::to_wstring(errorList.size() - 3) + L"개 파일 오류";
                        break;
                    }
                }
                *outErrorMessage = combined;
            }
            return false;
        }

        return true;
    }

    static void CollectFilesRecursive(
        const std::wstring& rootDir,
        const std::wstring& relPrefix,
        std::vector<std::pair<std::wstring, std::wstring>>& outList)
    {
        std::wstring searchPattern = PathHelper::Combine(rootDir, L"*");
        WIN32_FIND_DATAW fd;
        HANDLE hFind = FindFirstFileW(searchPattern.c_str(), &fd);
        if (hFind == INVALID_HANDLE_VALUE) return;

        do
        {
            if (wcscmp(fd.cFileName, L".") == 0 || wcscmp(fd.cFileName, L"..") == 0)
                continue;

            std::wstring full = PathHelper::Combine(rootDir, fd.cFileName);
            std::wstring rel = relPrefix.empty() ? fd.cFileName : (relPrefix + L"\\" + fd.cFileName);

            if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
            {
                CollectFilesRecursive(full, rel, outList);
            }
            else
            {
                outList.push_back({ full, rel });
            }
        } while (FindNextFileW(hFind, &fd));

        FindClose(hFind);
    }

    bool ArchiveEngine::Compress(
        const std::vector<std::wstring>& sourcePaths,
        const std::wstring& destArchive,
        const std::wstring& format,
        int level,
        ProgressCallback onProgress,
        CancelCallback isCanceled,
        std::wstring* outErrorMessage)
    {
        PathHelper::EnsureDirectoryExists(PathHelper::GetDirectory(destArchive));

        std::vector<std::pair<std::wstring, std::wstring>> filesToPack;
        uint64_t totalBytes = 0;

        for (const auto& src : sourcePaths)
        {
            if (PathHelper::FileExists(src))
            {
                WIN32_FILE_ATTRIBUTE_DATA fad;
                if (GetFileAttributesExW(src.c_str(), GetFileExInfoStandard, &fad))
                {
                    uint64_t sz = (static_cast<uint64_t>(fad.nFileSizeHigh) << 32) | fad.nFileSizeLow;
                    totalBytes += sz;
                }
                filesToPack.push_back({ src, PathHelper::GetFileName(src) });
            }
            else if (PathHelper::DirectoryExists(src))
            {
                std::wstring folderName = PathHelper::GetFileName(src);
                CollectFilesRecursive(src, folderName, filesToPack);
            }
        }

        if (filesToPack.empty())
        {
            if (outErrorMessage) *outErrorMessage = L"압축할 파일이 선택되지 않았습니다.";
            return false;
        }

        // Calculate total bytes
        totalBytes = 0;
        for (const auto& item : filesToPack)
        {
            WIN32_FILE_ATTRIBUTE_DATA fad;
            if (GetFileAttributesExW(item.first.c_str(), GetFileExInfoStandard, &fad))
            {
                uint64_t sz = (static_cast<uint64_t>(fad.nFileSizeHigh) << 32) | fad.nFileSizeLow;
                totalBytes += sz;
            }
        }

        struct archive* a = archive_write_new();
        if (format == L"7z")
        {
            archive_write_set_format_7zip(a);
        }
        else if (format == L"tar")
        {
            archive_write_set_format_gnutar(a);
        }
        else
        {
            archive_write_set_format_zip(a);
            // Ensure UTF-8 filenames in ZIP archives
            archive_write_set_format_option(a, "zip", "hdrcharset", "UTF-8");
            if (level == 0)
            {
                archive_write_set_format_option(a, "zip", "compression", "store");
            }
            else if (level <= 3)
            {
                archive_write_set_format_option(a, "zip", "compression", "fast");
            }
            else
            {
                archive_write_set_format_option(a, "zip", "compression", "deflate");
            }
        }

        if (archive_write_open_filename_w(a, destArchive.c_str()) != ARCHIVE_OK)
        {
            if (outErrorMessage)
            {
                const char* err = archive_error_string(a);
                *outErrorMessage = err ? PathHelper::Utf8ToWide(err) : L"대상 아카이브를 생성할 수 없습니다.";
            }
            archive_write_free(a);
            return false;
        }

        uint64_t processedBytes = 0;
        uint32_t processedFiles = 0;
        uint32_t totalFiles = static_cast<uint32_t>(filesToPack.size());
        auto startTime = std::chrono::steady_clock::now();

        std::vector<char> buffer(512 * 1024); // 512KB chunk buffer

        for (const auto& item : filesToPack)
        {
            if (isCanceled && isCanceled())
            {
                archive_write_close(a);
                archive_write_free(a);
                DeleteFileW(destArchive.c_str());
                if (outErrorMessage) *outErrorMessage = L"사용자에 의해 작업이 취소되었습니다.";
                return false;
            }

            HANDLE hFile = CreateFileW(
                item.first.c_str(),
                GENERIC_READ,
                FILE_SHARE_READ,
                NULL,
                OPEN_EXISTING,
                FILE_ATTRIBUTE_NORMAL,
                NULL);

            if (hFile == INVALID_HANDLE_VALUE) continue;

            LARGE_INTEGER liSize;
            GetFileSizeEx(hFile, &liSize);

            FILETIME ftWrite;
            GetFileTime(hFile, NULL, NULL, &ftWrite);
            ULARGE_INTEGER uli;
            uli.LowPart = ftWrite.dwLowDateTime;
            uli.HighPart = ftWrite.dwHighDateTime;
            time_t unixTime = static_cast<time_t>((uli.QuadPart - 116444736000000000) / 10000000);

            struct archive_entry* entry = archive_entry_new();
            archive_entry_copy_pathname_w(entry, item.second.c_str());
            archive_entry_set_size(entry, liSize.QuadPart);
            archive_entry_set_filetype(entry, AE_IFREG);
            archive_entry_set_perm(entry, 0644);
            archive_entry_set_mtime(entry, unixTime, 0);

            archive_write_header(a, entry);

            DWORD bytesRead = 0;
            while (ReadFile(hFile, buffer.data(), static_cast<DWORD>(buffer.size()), &bytesRead, NULL) && bytesRead > 0)
            {
                if (isCanceled && isCanceled())
                {
                    CloseHandle(hFile);
                    archive_entry_free(entry);
                    archive_write_close(a);
                    archive_write_free(a);
                    DeleteFileW(destArchive.c_str());
                    if (outErrorMessage) *outErrorMessage = L"사용자에 의해 작업이 취소되었습니다.";
                    return false;
                }

                archive_write_data(a, buffer.data(), bytesRead);
                processedBytes += bytesRead;

                if (onProgress)
                {
                    auto now = std::chrono::steady_clock::now();
                    double elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - startTime).count() / 1000.0;
                    double speed = (elapsed > 0.05) ? (processedBytes / elapsed) : 0.0;
                    double remaining = (speed > 0.0 && totalBytes > processedBytes) ? ((totalBytes - processedBytes) / speed) : 0.0;

                    ProgressInfo info;
                    info.currentFile = PathHelper::GetFileName(item.second);
                    info.processedBytes = processedBytes;
                    info.totalBytes = totalBytes > 0 ? totalBytes : processedBytes;
                    info.processedFiles = processedFiles;
                    info.totalFiles = totalFiles;
                    info.speedBytesPerSec = speed;
                    info.remainingSeconds = remaining;
                    info.statusMessage = L"압축하는 중: " + info.currentFile;
                    onProgress(info);
                }
            }

            CloseHandle(hFile);
            archive_entry_free(entry);
            processedFiles++;
        }

        archive_write_close(a);
        archive_write_free(a);

        if (onProgress)
        {
            ProgressInfo doneInfo;
            doneInfo.currentFile = L"완료";
            doneInfo.processedBytes = processedBytes;
            doneInfo.totalBytes = totalBytes > 0 ? totalBytes : processedBytes;
            doneInfo.processedFiles = processedFiles;
            doneInfo.totalFiles = totalFiles;
            doneInfo.speedBytesPerSec = 0.0;
            doneInfo.remainingSeconds = 0.0;
            doneInfo.statusMessage = L"압축 완료";
            onProgress(doneInfo);
        }

        return true;
    }

    bool ArchiveEngine::TestArchive(
        const std::wstring& archivePath,
        ProgressCallback onProgress,
        CancelCallback isCanceled,
        std::wstring* outErrorMessage,
        const std::string& codepage)
    {
        struct archive* a = archive_read_new();
        archive_read_support_filter_all(a);
        archive_read_support_format_all(a);
        ConfigureReaderCharset(a, codepage);

        if (OpenArchiveRead(a, archivePath, 1024 * 512) != ARCHIVE_OK)
        {
            if (outErrorMessage)
            {
                const char* err = archive_error_string(a);
                *outErrorMessage = err ? PathHelper::Utf8ToWide(err) : L"아카이브를 열 수 없습니다.";
            }
            archive_read_free(a);
            return false;
        }

        UINT cpId = EncodingHelper::NameToCodepage(ResolveCodepage(codepage));
        struct archive_entry* entry = nullptr;
        uint32_t count = 0;
        std::vector<char> dummy(256 * 1024);

        while (archive_read_next_header(a, &entry) == ARCHIVE_OK)
        {
            if (isCanceled && isCanceled())
            {
                archive_read_free(a);
                if (outErrorMessage) *outErrorMessage = L"테스트가 취소되었습니다.";
                return false;
            }

            std::wstring p = GetEntryPath(entry, cpId);
            while (archive_read_data(a, dummy.data(), dummy.size()) > 0)
            {
                // read through
            }
            count++;

            if (onProgress)
            {
                ProgressInfo info;
                info.currentFile = PathHelper::GetFileName(p);
                info.processedFiles = count;
                info.statusMessage = L"테스트 중: " + info.currentFile;
                onProgress(info);
            }
        }

        archive_read_free(a);
        return true;
    }
}

