#include "CompressDlg.h"
#include "AppSettings.h"
#include "PathHelper.h"
#include "res/resource.h"
#include <commdlg.h>

namespace EasyZip
{
    struct DialogState
    {
        CompressOptions* options = nullptr;
        bool confirmed = false;
    };

    bool CompressDlg::Show(HWND hParent, const std::vector<std::wstring>& sourcePaths, CompressOptions& outOptions)
    {
        if (sourcePaths.empty()) return false;

        outOptions.sourcePaths = sourcePaths;

        // Determine default destination archive path
        std::wstring parentDir = PathHelper::GetDirectory(sourcePaths[0]);
        std::wstring baseName;
        if (sourcePaths.size() == 1)
        {
            baseName = PathHelper::GetFileNameWithoutExtension(sourcePaths[0]);
        }
        else
        {
            baseName = PathHelper::GetFileName(parentDir);
            if (baseName.empty() || baseName.find(L':') != std::wstring::npos)
            {
                baseName = L"Archive";
            }
        }

        outOptions.format = AppSettings::Instance().GetDefaultFormat();
        outOptions.level = AppSettings::Instance().GetDefaultLevel();
        outOptions.threads = AppSettings::Instance().GetDefaultThreads();
        outOptions.destArchivePath = PathHelper::GetUniqueFilePath(parentDir, baseName, L"." + outOptions.format);

        DialogState state;
        state.options = &outOptions;

        INT_PTR ret = DialogBoxParamW(
            GetModuleHandleW(NULL),
            MAKEINTRESOURCEW(IDD_COMPRESS),
            hParent,
            DlgProc,
            reinterpret_cast<LPARAM>(&state));

        return (ret == IDOK && state.confirmed);
    }

    INT_PTR CALLBACK CompressDlg::DlgProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam)
    {
        DialogState* pState = reinterpret_cast<DialogState*>(GetWindowLongPtrW(hDlg, DWLP_USER));

        switch (msg)
        {
        case WM_INITDIALOG:
        {
            pState = reinterpret_cast<DialogState*>(lParam);
            SetWindowLongPtrW(hDlg, DWLP_USER, reinterpret_cast<LONG_PTR>(pState));

            if (!pState || !pState->options) return TRUE;

            // Summary text
            std::wstring summary;
            if (pState->options->sourcePaths.size() == 1)
            {
                summary = PathHelper::GetFileName(pState->options->sourcePaths[0]);
            }
            else
            {
                summary = PathHelper::GetFileName(pState->options->sourcePaths[0]) +
                    L" 외 " + std::to_wstring(pState->options->sourcePaths.size() - 1) + L"개 파일/폴더";
            }
            SetDlgItemTextW(hDlg, IDC_LBL_SOURCE_SUMMARY, summary.c_str());

            // Destination path
            SetDlgItemTextW(hDlg, IDC_EDT_DEST_PATH, pState->options->destArchivePath.c_str());

            // Formats combo
            HWND hFmt = GetDlgItem(hDlg, IDC_CMB_FORMAT);
            SendMessageW(hFmt, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"ZIP"));
            SendMessageW(hFmt, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"7Z"));
            SendMessageW(hFmt, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"TAR"));
            if (pState->options->format == L"7z") SendMessageW(hFmt, CB_SETCURSEL, 1, 0);
            else if (pState->options->format == L"tar") SendMessageW(hFmt, CB_SETCURSEL, 2, 0);
            else SendMessageW(hFmt, CB_SETCURSEL, 0, 0);

            // Levels combo
            HWND hLvl = GetDlgItem(hDlg, IDC_CMB_LEVEL);
            SendMessageW(hLvl, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"저장 (압축 안 함)"));
            SendMessageW(hLvl, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"가장 빠름 (1)"));
            SendMessageW(hLvl, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"보통 (5)"));
            SendMessageW(hLvl, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"최대 압축 (7)"));
            SendMessageW(hLvl, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"울트라 압축 (9)"));
            if (pState->options->level == 0) SendMessageW(hLvl, CB_SETCURSEL, 0, 0);
            else if (pState->options->level <= 3) SendMessageW(hLvl, CB_SETCURSEL, 1, 0);
            else if (pState->options->level <= 5) SendMessageW(hLvl, CB_SETCURSEL, 2, 0);
            else if (pState->options->level <= 7) SendMessageW(hLvl, CB_SETCURSEL, 3, 0);
            else SendMessageW(hLvl, CB_SETCURSEL, 4, 0);

            // Split sizes
            HWND hSplit = GetDlgItem(hDlg, IDC_CMB_SPLIT_SIZE);
            SendMessageW(hSplit, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"분할 안 함"));
            SendMessageW(hSplit, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"10 MB (이메일)"));
            SendMessageW(hSplit, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"100 MB"));
            SendMessageW(hSplit, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"700 MB (CD)"));
            SendMessageW(hSplit, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"4.3 GB (DVD)"));
            SendMessageW(hSplit, CB_SETCURSEL, 0, 0);

            // Threads
            HWND hThreads = GetDlgItem(hDlg, IDC_CMB_THREADS);
            SendMessageW(hThreads, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"자동 (CPU 코어 전체)"));
            SendMessageW(hThreads, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"1 스레드"));
            SendMessageW(hThreads, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"2 스레드"));
            SendMessageW(hThreads, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"4 스레드"));
            SendMessageW(hThreads, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"8 스레드"));
            SendMessageW(hThreads, CB_SETCURSEL, 0, 0);

            return TRUE;
        }

        case WM_COMMAND:
            switch (LOWORD(wParam))
            {
            case IDC_BTN_BROWSE_DEST:
            {
                wchar_t fileBuf[MAX_PATH * 2] = { 0 };
                GetDlgItemTextW(hDlg, IDC_EDT_DEST_PATH, fileBuf, _countof(fileBuf));

                OPENFILENAMEW ofn = { 0 };
                ofn.lStructSize = sizeof(ofn);
                ofn.hwndOwner = hDlg;
                ofn.lpstrFilter = L"ZIP 아카이브 (*.zip)\0*.zip\0"
                                  L"7-Zip 아카이브 (*.7z)\0*.7z\0"
                                  L"TAR 아카이브 (*.tar)\0*.tar\0"
                                  L"모든 파일 (*.*)\0*.*\0";
                ofn.lpstrFile = fileBuf;
                ofn.nMaxFile = _countof(fileBuf);
                ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;

                if (GetSaveFileNameW(&ofn))
                {
                    SetDlgItemTextW(hDlg, IDC_EDT_DEST_PATH, fileBuf);
                }
                return TRUE;
            }

            case IDC_CMB_FORMAT:
                if (HIWORD(wParam) == CBN_SELCHANGE)
                {
                    int sel = static_cast<int>(SendDlgItemMessageW(hDlg, IDC_CMB_FORMAT, CB_GETCURSEL, 0, 0));
                    std::wstring ext = (sel == 1) ? L".7z" : (sel == 2 ? L".tar" : L".zip");

                    wchar_t current[MAX_PATH * 2] = { 0 };
                    GetDlgItemTextW(hDlg, IDC_EDT_DEST_PATH, current, _countof(current));
                    std::wstring base = PathHelper::GetFileNameWithoutExtension(current);
                    std::wstring dir = PathHelper::GetDirectory(current);
                    std::wstring updated = PathHelper::Combine(dir, base + ext);
                    SetDlgItemTextW(hDlg, IDC_EDT_DEST_PATH, updated.c_str());
                }
                return TRUE;

            case IDOK:
            {
                if (!pState || !pState->options) return TRUE;

                wchar_t pathBuf[MAX_PATH * 2] = { 0 };
                GetDlgItemTextW(hDlg, IDC_EDT_DEST_PATH, pathBuf, _countof(pathBuf));
                if (wcslen(pathBuf) == 0)
                {
                    MessageBoxW(hDlg, L"저장 파일명을 입력해 주세요.", L"알림", MB_ICONWARNING);
                    return TRUE;
                }
                pState->options->destArchivePath = pathBuf;

                int fmtSel = static_cast<int>(SendDlgItemMessageW(hDlg, IDC_CMB_FORMAT, CB_GETCURSEL, 0, 0));
                pState->options->format = (fmtSel == 1) ? L"7z" : (fmtSel == 2 ? L"tar" : L"zip");

                int lvlSel = static_cast<int>(SendDlgItemMessageW(hDlg, IDC_CMB_LEVEL, CB_GETCURSEL, 0, 0));
                int lvls[] = { 0, 1, 5, 7, 9 };
                pState->options->level = (lvlSel >= 0 && lvlSel < 5) ? lvls[lvlSel] : 5;

                wchar_t passBuf[128] = { 0 };
                GetDlgItemTextW(hDlg, IDC_EDT_PASSWORD, passBuf, _countof(passBuf));
                pState->options->password = passBuf;

                pState->confirmed = true;
                EndDialog(hDlg, IDOK);
                return TRUE;
            }

            case IDCANCEL:
                EndDialog(hDlg, IDCANCEL);
                return TRUE;
            }
            break;
        }

        return FALSE;
    }
}

