#pragma once

#include <windows.h>
#include <string>
#include <atomic>
#include "ArchiveEngine.h"

namespace EasyZip
{
    class ProgressDlg
    {
    public:
        ProgressDlg(HWND hParent, const std::wstring& title);
        ~ProgressDlg();

        void Show();
        void Close();

        void UpdateProgress(const ProgressInfo& info);
        bool IsCanceled() const { return m_bCanceled.load(); }
        bool IsPaused() const { return m_bPaused.load(); }
        bool ShouldOpenFolderAfterExtract() const { return m_bOpenFolder; }
        void SetDestinationFolder(const std::wstring& folder) { m_destFolder = folder; }

        HWND GetHwnd() const { return m_hDlg; }

    private:
        static INT_PTR CALLBACK DlgProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam);

        HWND m_hParent = NULL;
        HWND m_hDlg = NULL;
        std::wstring m_title;
        std::wstring m_destFolder;
        std::atomic<bool> m_bCanceled{ false };
        std::atomic<bool> m_bPaused{ false };
        bool m_bOpenFolder = false;
    };
}

