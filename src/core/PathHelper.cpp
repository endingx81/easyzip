#include "PathHelper.h"
#include <shlwapi.h>
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <regex>
#include <unordered_set>

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

        // 1. Check modern multi-part pattern: <base>.part(\d+).<ext> (e.g. .part1.rar, .part02.rar, .part001.rar)
        static const std::wregex partRegex(LR"(^(.*)\.part\d+\.[a-zA-Z0-9]+$)", std::regex_constants::icase);
        std::wsmatch m;
        if (std::regex_match(name, m, partRegex))
        {
            return m[1].str();
        }

        // 2. Check numeric split extension: <base>.(7z|zip|tar)\.(\d{3,})$ or <base>\.(\d{3,})$
        static const std::wregex numSplitRegex(LR"(^(.*)\.(?:7z|zip|tar)\.\d{3,}$)", std::regex_constants::icase);
        if (std::regex_match(name, m, numSplitRegex))
        {
            return m[1].str();
        }

        static const std::vector<std::wstring> compoundExts = {
            L".tar.gz", L".tar.bz2", L".tar.xz", L".tar.zst",
            L".7z.001", L".zip.001"
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

    std::vector<std::wstring> PathHelper::GetVolumeFiles(const std::wstring& archivePath)
    {
        std::vector<std::wstring> defaultResult = { archivePath };
        std::wstring dir = GetDirectory(archivePath);
        std::wstring fileName = GetFileName(archivePath);
        if (fileName.empty()) return defaultResult;

        // 1. Check modern multi-part pattern: <base>.part(\d+).<ext> (e.g. .part1.rar, .part01.rar, .part1.7z)
        static const std::wregex partRegex(LR"(^(.*)\.part(\d+)\.([a-zA-Z0-9]+)$)", std::regex_constants::icase);
        std::wsmatch m;
        if (std::regex_match(fileName, m, partRegex))
        {
            std::wstring basePrefix = m[1].str();
            std::wstring ext = m[3].str();
            std::wstring searchPattern = Combine(dir, basePrefix + L".part*." + ext);

            WIN32_FIND_DATAW ffd;
            HANDLE hFind = FindFirstFileW(searchPattern.c_str(), &ffd);
            if (hFind != INVALID_HANDLE_VALUE)
            {
                std::vector<std::pair<int, std::wstring>> foundParts;
                do
                {
                    if (!(ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
                    {
                        std::wstring curName = ffd.cFileName;
                        std::wsmatch curMatch;
                        if (std::regex_match(curName, curMatch, partRegex))
                        {
                            if (_wcsicmp(curMatch[1].str().c_str(), basePrefix.c_str()) == 0 &&
                                _wcsicmp(curMatch[3].str().c_str(), ext.c_str()) == 0)
                            {
                                int partNum = _wtoi(curMatch[2].str().c_str());
                                foundParts.push_back({ partNum, Combine(dir, curName) });
                            }
                        }
                    }
                } while (FindNextFileW(hFind, &ffd));
                FindClose(hFind);

                if (foundParts.size() > 1)
                {
                    std::sort(foundParts.begin(), foundParts.end(), [](const auto& a, const auto& b) {
                        return a.first < b.first;
                    });
                    std::vector<std::wstring> result;
                    result.reserve(foundParts.size());
                    for (const auto& p : foundParts)
                    {
                        result.push_back(p.second);
                    }
                    return result;
                }
            }
        }

        // 2. Check numeric split extension pattern: <base>.(\d{3,}) (e.g. .7z.001, .zip.001, .001)
        static const std::wregex numSplitRegex(LR"(^(.*)\.(\d{3,})$)", std::regex_constants::icase);
        if (std::regex_match(fileName, m, numSplitRegex))
        {
            std::wstring basePrefix = m[1].str();
            std::wstring searchPattern = Combine(dir, basePrefix + L".*");

            WIN32_FIND_DATAW ffd;
            HANDLE hFind = FindFirstFileW(searchPattern.c_str(), &ffd);
            if (hFind != INVALID_HANDLE_VALUE)
            {
                std::vector<std::pair<int, std::wstring>> foundParts;
                do
                {
                    if (!(ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
                    {
                        std::wstring curName = ffd.cFileName;
                        std::wsmatch curMatch;
                        if (std::regex_match(curName, curMatch, numSplitRegex))
                        {
                            if (_wcsicmp(curMatch[1].str().c_str(), basePrefix.c_str()) == 0)
                            {
                                int partNum = _wtoi(curMatch[2].str().c_str());
                                foundParts.push_back({ partNum, Combine(dir, curName) });
                            }
                        }
                    }
                } while (FindNextFileW(hFind, &ffd));
                FindClose(hFind);

                if (foundParts.size() > 1)
                {
                    std::sort(foundParts.begin(), foundParts.end(), [](const auto& a, const auto& b) {
                        return a.first < b.first;
                    });
                    std::vector<std::wstring> result;
                    result.reserve(foundParts.size());
                    for (const auto& p : foundParts)
                    {
                        result.push_back(p.second);
                    }
                    return result;
                }
            }
        }

        // 3. Check legacy WinRAR multi-volume: <base>.rar, <base>.r00, <base>.r01 ...
        static const std::wregex rarLegacyRegex(LR"(^(.*)\.(rar|r\d{2,})$)", std::regex_constants::icase);
        if (std::regex_match(fileName, m, rarLegacyRegex))
        {
            std::wstring basePrefix = m[1].str();
            std::wstring rarFile = Combine(dir, basePrefix + L".rar");
            std::wstring searchPattern = Combine(dir, basePrefix + L".r*");

            WIN32_FIND_DATAW ffd;
            HANDLE hFind = FindFirstFileW(searchPattern.c_str(), &ffd);
            if (hFind != INVALID_HANDLE_VALUE)
            {
                std::vector<std::pair<int, std::wstring>> foundRParts;
                static const std::wregex rNumRegex(LR"(^(.*)\.r(\d{2,})$)", std::regex_constants::icase);
                do
                {
                    if (!(ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
                    {
                        std::wstring curName = ffd.cFileName;
                        std::wsmatch curMatch;
                        if (std::regex_match(curName, curMatch, rNumRegex))
                        {
                            if (_wcsicmp(curMatch[1].str().c_str(), basePrefix.c_str()) == 0)
                            {
                                int partNum = _wtoi(curMatch[2].str().c_str());
                                foundRParts.push_back({ partNum, Combine(dir, curName) });
                            }
                        }
                    }
                } while (FindNextFileW(hFind, &ffd));
                FindClose(hFind);

                if (!foundRParts.empty())
                {
                    std::sort(foundRParts.begin(), foundRParts.end(), [](const auto& a, const auto& b) {
                        return a.first < b.first;
                    });
                    std::vector<std::wstring> result;
                    if (FileExists(rarFile))
                    {
                        result.push_back(rarFile);
                    }
                    for (const auto& p : foundRParts)
                    {
                        result.push_back(p.second);
                    }
                    if (result.size() > 1) return result;
                }
            }
        }

        // 4. Check split PKZIP: <base>.z01, <base>.z02 ... <base>.zip
        static const std::wregex zipSplitRegex(LR"(^(.*)\.(zip|z\d{2,})$)", std::regex_constants::icase);
        if (std::regex_match(fileName, m, zipSplitRegex))
        {
            std::wstring basePrefix = m[1].str();
            std::wstring zipFile = Combine(dir, basePrefix + L".zip");
            std::wstring searchPattern = Combine(dir, basePrefix + L".z*");

            WIN32_FIND_DATAW ffd;
            HANDLE hFind = FindFirstFileW(searchPattern.c_str(), &ffd);
            if (hFind != INVALID_HANDLE_VALUE)
            {
                std::vector<std::pair<int, std::wstring>> foundZParts;
                static const std::wregex zNumRegex(LR"(^(.*)\.z(\d{2,})$)", std::regex_constants::icase);
                do
                {
                    if (!(ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
                    {
                        std::wstring curName = ffd.cFileName;
                        std::wsmatch curMatch;
                        if (std::regex_match(curName, curMatch, zNumRegex))
                        {
                            if (_wcsicmp(curMatch[1].str().c_str(), basePrefix.c_str()) == 0)
                            {
                                int partNum = _wtoi(curMatch[2].str().c_str());
                                foundZParts.push_back({ partNum, Combine(dir, curName) });
                            }
                        }
                    }
                } while (FindNextFileW(hFind, &ffd));
                FindClose(hFind);

                if (!foundZParts.empty())
                {
                    std::sort(foundZParts.begin(), foundZParts.end(), [](const auto& a, const auto& b) {
                        return a.first < b.first;
                    });
                    std::vector<std::wstring> result;
                    for (const auto& p : foundZParts)
                    {
                        result.push_back(p.second);
                    }
                    if (FileExists(zipFile))
                    {
                        result.push_back(zipFile);
                    }
                    if (result.size() > 1) return result;
                }
            }
        }

        return defaultResult;
    }

    std::vector<std::wstring> PathHelper::FilterMultiVolumeDuplicates(const std::vector<std::wstring>& archives)
    {
        std::vector<std::wstring> filtered;
        std::unordered_set<std::wstring> seenPrimaryVolumes;

        for (const auto& arc : archives)
        {
            std::vector<std::wstring> vols = GetVolumeFiles(arc);
            std::wstring primaryVol = vols.empty() ? arc : vols.front();

            std::wstring normPrimary = Normalize(primaryVol);
            std::transform(normPrimary.begin(), normPrimary.end(), normPrimary.begin(), ::towlower);

            if (seenPrimaryVolumes.find(normPrimary) == seenPrimaryVolumes.end())
            {
                seenPrimaryVolumes.insert(normPrimary);
                filtered.push_back(primaryVol);
            }
        }

        return filtered;
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

