#pragma once

#include <windows.h>
#include <string>
#include <vector>

namespace EasyZip
{
    struct CompressOptions
    {
        std::vector<std::wstring> sourcePaths;
        std::wstring destArchivePath;
        std::wstring format = L"zip";
        int level = 5;
        std::wstring password;
        int threads = 0;
    };

    class CompressDlg
    {
    public:
        static bool Show(HWND hParent, const std::vector<std::wstring>& sourcePaths, CompressOptions& outOptions);

    private:
        static INT_PTR CALLBACK DlgProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam);
    };
}

