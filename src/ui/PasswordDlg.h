#pragma once

#include <windows.h>
#include <string>

namespace EasyZip
{
    class PasswordDlg
    {
    public:
        // Returns true if user entered password and clicked OK.
        // Returns false if user canceled.
        static bool Prompt(HWND hParent, const std::wstring& archiveName, std::string& outUtf8Password);

    private:
        static INT_PTR CALLBACK DlgProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);
    };
}
