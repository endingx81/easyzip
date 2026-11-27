#include "PasswordDlg.h"
#include "res/resource.h"
#include "PathHelper.h"

namespace EasyZip
{
    struct PasswordPromptState
    {
        std::wstring archiveName;
        std::string* pOutPassword = nullptr;
        bool confirmed = false;
    };

    bool PasswordDlg::Prompt(HWND hParent, const std::wstring& archiveName, std::string& outUtf8Password)
    {
        PasswordPromptState state;
        state.archiveName = archiveName;
        state.pOutPassword = &outUtf8Password;
        state.confirmed = false;

        INT_PTR ret = DialogBoxParamW(
            GetModuleHandleW(NULL),
            MAKEINTRESOURCEW(IDD_PASSWORD),
            hParent,
            DlgProc,
            reinterpret_cast<LPARAM>(&state));

        return (ret == IDOK && state.confirmed);
    }

    INT_PTR CALLBACK PasswordDlg::DlgProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
    {
        PasswordPromptState* pState = reinterpret_cast<PasswordPromptState*>(GetWindowLongPtrW(hDlg, DWLP_USER));

        switch (uMsg)
        {
        case WM_INITDIALOG:
        {
            pState = reinterpret_cast<PasswordPromptState*>(lParam);
            SetWindowLongPtrW(hDlg, DWLP_USER, reinterpret_cast<LONG_PTR>(pState));

            if (pState && !pState->archiveName.empty())
            {
                std::wstring hint = L"['" + PathHelper::GetFileName(pState->archiveName) + L"'] 암호로 보호되어 있습니다.\n암호를 입력해 주세요:";
                SetDlgItemTextW(hDlg, IDC_LBL_PASSWORD_HINT, hint.c_str());
            }

            // Focus on password edit box
            SetFocus(GetDlgItem(hDlg, IDC_EDT_INPUT_PASSWORD));
            return FALSE;
        }

        case WM_COMMAND:
        {
            switch (LOWORD(wParam))
            {
            case IDC_CHK_SHOW_PASSWORD:
            {
                BOOL checked = (IsDlgButtonChecked(hDlg, IDC_CHK_SHOW_PASSWORD) == BST_CHECKED);
                HWND hEdit = GetDlgItem(hDlg, IDC_EDT_INPUT_PASSWORD);
                if (hEdit)
                {
                    // 0 removes password mask, non-zero sets mask char (default bullet is 0x25CF or L'*')
                    SendMessageW(hEdit, EM_SETPASSWORDCHAR, checked ? 0 : L'*', 0);
                    InvalidateRect(hEdit, NULL, TRUE);
                }
                return TRUE;
            }

            case IDOK:
            {
                if (pState && pState->pOutPassword)
                {
                    wchar_t buf[512] = { 0 };
                    GetDlgItemTextW(hDlg, IDC_EDT_INPUT_PASSWORD, buf, _countof(buf));
                    *(pState->pOutPassword) = PathHelper::WideToUtf8(buf);
                    pState->confirmed = true;
                }
                EndDialog(hDlg, IDOK);
                return TRUE;
            }

            case IDCANCEL:
            {
                EndDialog(hDlg, IDCANCEL);
                return TRUE;
            }
            }
            break;
        }
        }
        return FALSE;
    }
}
