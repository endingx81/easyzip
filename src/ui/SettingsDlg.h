#pragma once

#include <windows.h>

namespace EasyZip
{
    class SettingsDlg
    {
    public:
        static void Show(HWND hParent);

    private:
        static INT_PTR CALLBACK DlgProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam);
        static void SwitchTab(HWND hDlg, int tabIndex);
    };
}

