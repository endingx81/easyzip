#include "ProgressDlg.h"
#include "AppSettings.h"
#include "PathHelper.h"
#include "res/resource.h"
#include <commctrl.h>
#include <shellapi.h>

namespace EasyZip
{
    ProgressDlg::ProgressDlg(HWND hParent, const std::wstring& title)
        : m_hParent(hParent), m_title(title)
    {
        m_bOpenFolder = AppSettings::Instance().GetOpenFolderAfterExtract();
    }

    ProgressDlg::~ProgressDlg()
    {
        Close();
    }

    void ProgressDlg::Show()
    {
        m_hDlg = CreateDialogParamW(
            GetModuleHandleW(NULL),
            MAKEINTRESOURCEW(IDD_PROGRESS),
            m_hParent,
            DlgProc,
            reinterpret_cast<LPARAM>(this));

        if (m_hDlg)
        {
            if (!m_title.empty())
            {
                SetWindowTextW(m_hDlg, m_title.c_str());
            }

            SendDlgItemMessageW(m_hDlg, IDC_PROG_BAR, PBM_SETRANGE32, 0, 1000);
            SendDlgItemMessageW(m_hDlg, IDC_PROG_BAR, PBM_SETPOS, 0, 0);

            CheckDlgButton(m_hDlg, IDC_CHK_OPEN_FOLDER, m_bOpenFolder ? BST_CHECKED : BST_UNCHECKED);

            ShowWindow(m_hDlg, SW_SHOW);
            UpdateWindow(m_hDlg);
        }
    }

    void ProgressDlg::Close()
    {
        if (m_hDlg)
        {
            if (m_bOpenFolder && !m_bCanceled.load() && !m_destFolder.empty())
            {
                ShellExecuteW(NULL, L"open", m_destFolder.c_str(), NULL, NULL, SW_SHOWNORMAL);
            }

            DestroyWindow(m_hDlg);
            m_hDlg = NULL;
        }
    }

    void ProgressDlg::UpdateProgress(const ProgressInfo& info)
    {
        if (!m_hDlg) return;

        // Update progress bar
        int pos = 0;
        if (info.totalBytes > 0)
        {
            pos = static_cast<int>((static_cast<double>(info.processedBytes) / info.totalBytes) * 1000.0);
        }
        else if (info.totalFiles > 0)
        {
            pos = static_cast<int>((static_cast<double>(info.processedFiles) / info.totalFiles) * 1000.0);
        }
        if (pos > 1000) pos = 1000;
        SendDlgItemMessageW(m_hDlg, IDC_PROG_BAR, PBM_SETPOS, pos, 0);

        // Status text
        std::wstring status = info.statusMessage.empty() ? (L"처리 중: " + info.currentFile) : info.statusMessage;
        SetDlgItemTextW(m_hDlg, IDC_LBL_STATUS, status.c_str());

        // Processed count & bytes
        std::wstring procStr = L"진행: " + std::to_wstring(info.processedFiles) + L" / " +
            std::to_wstring(info.totalFiles) + L" 파일 (" +
            PathHelper::FormatFileSize(info.processedBytes) + L" / " +
            PathHelper::FormatFileSize(info.totalBytes) + L")";
        SetDlgItemTextW(m_hDlg, IDC_LBL_PROCESSED, procStr.c_str());

        // Speed
        std::wstring speedStr = L"속도: " + PathHelper::FormatSpeed(info.speedBytesPerSec);
        SetDlgItemTextW(m_hDlg, IDC_LBL_SPEED, speedStr.c_str());

        // ETA
        std::wstring etaStr = L"남은 시간: " + PathHelper::FormatTime(info.remainingSeconds);
        SetDlgItemTextW(m_hDlg, IDC_LBL_ETA, etaStr.c_str());

        // Process pending Windows messages so dialog stays responsive
        MSG msg;
        while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE))
        {
            if (!IsDialogMessageW(m_hDlg, &msg))
            {
                TranslateMessage(&msg);
                DispatchMessageW(&msg);
            }
        }
    }

    INT_PTR CALLBACK ProgressDlg::DlgProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam)
    {
        ProgressDlg* pThis = reinterpret_cast<ProgressDlg*>(GetWindowLongPtrW(hDlg, DWLP_USER));

        switch (msg)
        {
        case WM_INITDIALOG:
            pThis = reinterpret_cast<ProgressDlg*>(lParam);
            SetWindowLongPtrW(hDlg, DWLP_USER, reinterpret_cast<LONG_PTR>(pThis));
            return TRUE;

        case WM_COMMAND:
            switch (LOWORD(wParam))
            {
            case IDC_CHK_OPEN_FOLDER:
                if (pThis)
                {
                    bool checked = (IsDlgButtonChecked(hDlg, IDC_CHK_OPEN_FOLDER) == BST_CHECKED);
                    pThis->m_bOpenFolder = checked;
                    AppSettings::Instance().SetOpenFolderAfterExtract(checked);
                }
                return TRUE;

            case IDC_BTN_PAUSE:
                if (pThis)
                {
                    bool paused = !pThis->m_bPaused.load();
                    pThis->m_bPaused.store(paused);
                    SetDlgItemTextW(hDlg, IDC_BTN_PAUSE, paused ? L"계속" : L"일시정지");
                }
                return TRUE;

            case IDC_BTN_CANCEL:
            case IDCANCEL:
                if (pThis)
                {
                    pThis->m_bCanceled.store(true);
                    SetDlgItemTextW(hDlg, IDC_LBL_STATUS, L"취소하는 중...");
                    EnableWindow(GetDlgItem(hDlg, IDC_BTN_CANCEL), FALSE);
                }
                return TRUE;
            }
            break;

        case WM_CLOSE:
            if (pThis)
            {
                pThis->m_bCanceled.store(true);
            }
            return TRUE;
        }

        return FALSE;
    }
}

