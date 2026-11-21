#include "SettingsDlg.h"
#include "AppSettings.h"
#include "FileAssociation.h"
#include "res/resource.h"
#include <commctrl.h>
#include <uxtheme.h>

#pragma comment(lib, "uxtheme.lib")

namespace EasyZip
{
    static const int s_tab0Controls[] = {
        IDC_GRP_ASSOC,
        IDC_CHK_EXT_ZIP, IDC_CHK_EXT_7Z, IDC_CHK_EXT_RAR, IDC_CHK_EXT_ALZ, IDC_CHK_EXT_EGG,
        IDC_CHK_EXT_TAR, IDC_CHK_EXT_GZ, IDC_CHK_EXT_BZ2, IDC_CHK_EXT_XZ, IDC_CHK_EXT_ISO,
        IDC_LBL_WIN11_HINT,
        IDC_BTN_SELECT_ALL_ASSOC, IDC_BTN_CLEAR_ALL_ASSOC, IDC_BTN_WIN11_DEFAULT_APPS
    };

    static const int s_tab1Controls[] = {
        IDC_GRP_SHELL,
        IDC_CHK_ENABLE_SHELL_MENU,
        IDC_LBL_SHELL_ITEMS,
        IDC_CHK_MENU_NEW_FOLDER,
        IDC_CHK_MENU_EXTRACT_SMART,
        IDC_CHK_MENU_EXTRACT_HERE,
        IDC_CHK_MENU_EXTRACT_FOLDER,
        IDC_CHK_MENU_COMPRESS
    };

    static const int s_tab2Controls[] = {
        IDC_GRP_GENERAL,
        IDC_CHK_DEF_OPEN_FOLDER
    };

    void SettingsDlg::Show(HWND hParent)
    {
        DialogBoxParamW(
            GetModuleHandleW(NULL),
            MAKEINTRESOURCEW(IDD_SETTINGS),
            hParent,
            DlgProc,
            0);
    }

    void SettingsDlg::SwitchTab(HWND hDlg, int tabIndex)
    {
        for (int id : s_tab0Controls)
        {
            ShowWindow(GetDlgItem(hDlg, id), (tabIndex == 0) ? SW_SHOW : SW_HIDE);
        }
        for (int id : s_tab1Controls)
        {
            ShowWindow(GetDlgItem(hDlg, id), (tabIndex == 1) ? SW_SHOW : SW_HIDE);
        }
        for (int id : s_tab2Controls)
        {
            ShowWindow(GetDlgItem(hDlg, id), (tabIndex == 2) ? SW_SHOW : SW_HIDE);
        }
        InvalidateRect(hDlg, NULL, TRUE);
    }

    INT_PTR CALLBACK SettingsDlg::DlgProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam)
    {
        switch (msg)
        {
        case WM_INITDIALOG:
        {
            EnableThemeDialogTexture(hDlg, ETDT_ENABLETAB);

            HWND hTab = GetDlgItem(hDlg, IDC_TAB_SETTINGS);
            TCITEMW tie = { 0 };
            tie.mask = TCIF_TEXT;

            wchar_t tab1[] = L"파일 연결";
            tie.pszText = tab1;
            TabCtrl_InsertItem(hTab, 0, &tie);

            wchar_t tab2[] = L"셸 메뉴";
            tie.pszText = tab2;
            TabCtrl_InsertItem(hTab, 1, &tie);

            wchar_t tab3[] = L"일반 설정";
            tie.pszText = tab3;
            TabCtrl_InsertItem(hTab, 2, &tie);

            // Tab 0: File Associations
            CheckDlgButton(hDlg, IDC_CHK_EXT_ZIP, FileAssociation::IsExtensionRegistered(L".zip") ? BST_CHECKED : BST_UNCHECKED);
            CheckDlgButton(hDlg, IDC_CHK_EXT_7Z,  FileAssociation::IsExtensionRegistered(L".7z")  ? BST_CHECKED : BST_UNCHECKED);
            CheckDlgButton(hDlg, IDC_CHK_EXT_RAR, FileAssociation::IsExtensionRegistered(L".rar") ? BST_CHECKED : BST_UNCHECKED);
            CheckDlgButton(hDlg, IDC_CHK_EXT_ALZ, FileAssociation::IsExtensionRegistered(L".alz") ? BST_CHECKED : BST_UNCHECKED);
            CheckDlgButton(hDlg, IDC_CHK_EXT_EGG, FileAssociation::IsExtensionRegistered(L".egg") ? BST_CHECKED : BST_UNCHECKED);
            CheckDlgButton(hDlg, IDC_CHK_EXT_TAR, FileAssociation::IsExtensionRegistered(L".tar") ? BST_CHECKED : BST_UNCHECKED);
            CheckDlgButton(hDlg, IDC_CHK_EXT_GZ,  FileAssociation::IsExtensionRegistered(L".gz")  ? BST_CHECKED : BST_UNCHECKED);
            CheckDlgButton(hDlg, IDC_CHK_EXT_BZ2, FileAssociation::IsExtensionRegistered(L".bz2") ? BST_CHECKED : BST_UNCHECKED);
            CheckDlgButton(hDlg, IDC_CHK_EXT_XZ,  FileAssociation::IsExtensionRegistered(L".xz")  ? BST_CHECKED : BST_UNCHECKED);
            CheckDlgButton(hDlg, IDC_CHK_EXT_ISO, FileAssociation::IsExtensionRegistered(L".iso") ? BST_CHECKED : BST_UNCHECKED);

            // Tab 1: Shell Integration
            CheckDlgButton(hDlg, IDC_CHK_ENABLE_SHELL_MENU, AppSettings::Instance().GetEnableShellMenu() ? BST_CHECKED : BST_UNCHECKED);
            CheckDlgButton(hDlg, IDC_CHK_MENU_NEW_FOLDER, AppSettings::Instance().GetMenuNewFolder() ? BST_CHECKED : BST_UNCHECKED);
            CheckDlgButton(hDlg, IDC_CHK_MENU_EXTRACT_SMART, AppSettings::Instance().GetMenuExtractSmart() ? BST_CHECKED : BST_UNCHECKED);
            CheckDlgButton(hDlg, IDC_CHK_MENU_EXTRACT_HERE, AppSettings::Instance().GetMenuExtractHere() ? BST_CHECKED : BST_UNCHECKED);
            CheckDlgButton(hDlg, IDC_CHK_MENU_EXTRACT_FOLDER, AppSettings::Instance().GetMenuExtractFolder() ? BST_CHECKED : BST_UNCHECKED);
            CheckDlgButton(hDlg, IDC_CHK_MENU_COMPRESS, AppSettings::Instance().GetMenuCompress() ? BST_CHECKED : BST_UNCHECKED);

            // Tab 2: General Options
            CheckDlgButton(hDlg, IDC_CHK_DEF_OPEN_FOLDER, AppSettings::Instance().GetOpenFolderAfterExtract() ? BST_CHECKED : BST_UNCHECKED);

            SwitchTab(hDlg, 0);
            return TRUE;
        }

        case WM_NOTIFY:
        {
            NMHDR* pnm = reinterpret_cast<NMHDR*>(lParam);
            if (pnm->idFrom == IDC_TAB_SETTINGS && pnm->code == TCN_SELCHANGE)
            {
                int curTab = TabCtrl_GetCurSel(pnm->hwndFrom);
                SwitchTab(hDlg, curTab);
                return TRUE;
            }
            break;
        }

        case WM_CTLCOLORSTATIC:
        {
            HDC hdc = reinterpret_cast<HDC>(wParam);
            SetBkMode(hdc, TRANSPARENT);
            return reinterpret_cast<INT_PTR>(GetStockObject(NULL_BRUSH));
        }

        case WM_COMMAND:
            switch (LOWORD(wParam))
            {
            case IDC_BTN_SELECT_ALL_ASSOC:
                CheckDlgButton(hDlg, IDC_CHK_EXT_ZIP, BST_CHECKED);
                CheckDlgButton(hDlg, IDC_CHK_EXT_7Z,  BST_CHECKED);
                CheckDlgButton(hDlg, IDC_CHK_EXT_RAR, BST_CHECKED);
                CheckDlgButton(hDlg, IDC_CHK_EXT_ALZ, BST_CHECKED);
                CheckDlgButton(hDlg, IDC_CHK_EXT_EGG, BST_CHECKED);
                CheckDlgButton(hDlg, IDC_CHK_EXT_TAR, BST_CHECKED);
                CheckDlgButton(hDlg, IDC_CHK_EXT_GZ,  BST_CHECKED);
                CheckDlgButton(hDlg, IDC_CHK_EXT_BZ2, BST_CHECKED);
                CheckDlgButton(hDlg, IDC_CHK_EXT_XZ,  BST_CHECKED);
                CheckDlgButton(hDlg, IDC_CHK_EXT_ISO, BST_CHECKED);
                return TRUE;

            case IDC_BTN_CLEAR_ALL_ASSOC:
                CheckDlgButton(hDlg, IDC_CHK_EXT_ZIP, BST_UNCHECKED);
                CheckDlgButton(hDlg, IDC_CHK_EXT_7Z,  BST_UNCHECKED);
                CheckDlgButton(hDlg, IDC_CHK_EXT_RAR, BST_UNCHECKED);
                CheckDlgButton(hDlg, IDC_CHK_EXT_ALZ, BST_UNCHECKED);
                CheckDlgButton(hDlg, IDC_CHK_EXT_EGG, BST_UNCHECKED);
                CheckDlgButton(hDlg, IDC_CHK_EXT_TAR, BST_UNCHECKED);
                CheckDlgButton(hDlg, IDC_CHK_EXT_GZ,  BST_UNCHECKED);
                CheckDlgButton(hDlg, IDC_CHK_EXT_BZ2, BST_UNCHECKED);
                CheckDlgButton(hDlg, IDC_CHK_EXT_XZ,  BST_UNCHECKED);
                CheckDlgButton(hDlg, IDC_CHK_EXT_ISO, BST_UNCHECKED);
                return TRUE;

            case IDC_BTN_WIN11_DEFAULT_APPS:
            {
                auto saveAssoc = [&](int id, const wchar_t* ext) {
                    if (IsDlgButtonChecked(hDlg, id) == BST_CHECKED)
                        FileAssociation::RegisterExtension(ext);
                    else
                        FileAssociation::UnregisterExtension(ext);
                };
                saveAssoc(IDC_CHK_EXT_ZIP, L".zip");
                saveAssoc(IDC_CHK_EXT_7Z,  L".7z");
                saveAssoc(IDC_CHK_EXT_RAR, L".rar");
                saveAssoc(IDC_CHK_EXT_ALZ, L".alz");
                saveAssoc(IDC_CHK_EXT_EGG, L".egg");
                saveAssoc(IDC_CHK_EXT_TAR, L".tar");
                saveAssoc(IDC_CHK_EXT_GZ,  L".gz");
                saveAssoc(IDC_CHK_EXT_BZ2, L".bz2");
                saveAssoc(IDC_CHK_EXT_XZ,  L".xz");
                saveAssoc(IDC_CHK_EXT_ISO, L".iso");
                FileAssociation::RefreshShellIcons();

                FileAssociation::OpenWindowsDefaultAppsSettings();
                return TRUE;
            }

            case IDOK:
            {
                // Save associations
                auto saveAssoc = [&](int id, const wchar_t* ext) {
                    if (IsDlgButtonChecked(hDlg, id) == BST_CHECKED)
                        FileAssociation::RegisterExtension(ext);
                    else
                        FileAssociation::UnregisterExtension(ext);
                };

                saveAssoc(IDC_CHK_EXT_ZIP, L".zip");
                saveAssoc(IDC_CHK_EXT_7Z,  L".7z");
                saveAssoc(IDC_CHK_EXT_RAR, L".rar");
                saveAssoc(IDC_CHK_EXT_ALZ, L".alz");
                saveAssoc(IDC_CHK_EXT_EGG, L".egg");
                saveAssoc(IDC_CHK_EXT_TAR, L".tar");
                saveAssoc(IDC_CHK_EXT_GZ,  L".gz");
                saveAssoc(IDC_CHK_EXT_BZ2, L".bz2");
                saveAssoc(IDC_CHK_EXT_XZ,  L".xz");
                saveAssoc(IDC_CHK_EXT_ISO, L".iso");
                FileAssociation::RefreshShellIcons();

                // Save shell & general settings
                bool enableShell = (IsDlgButtonChecked(hDlg, IDC_CHK_ENABLE_SHELL_MENU) == BST_CHECKED);
                AppSettings::Instance().SetEnableShellMenu(enableShell);
                AppSettings::Instance().SetMenuNewFolder(IsDlgButtonChecked(hDlg, IDC_CHK_MENU_NEW_FOLDER) == BST_CHECKED);
                AppSettings::Instance().SetMenuExtractSmart(IsDlgButtonChecked(hDlg, IDC_CHK_MENU_EXTRACT_SMART) == BST_CHECKED);
                AppSettings::Instance().SetMenuExtractHere(IsDlgButtonChecked(hDlg, IDC_CHK_MENU_EXTRACT_HERE) == BST_CHECKED);
                AppSettings::Instance().SetMenuExtractFolder(IsDlgButtonChecked(hDlg, IDC_CHK_MENU_EXTRACT_FOLDER) == BST_CHECKED);
                AppSettings::Instance().SetMenuCompress(IsDlgButtonChecked(hDlg, IDC_CHK_MENU_COMPRESS) == BST_CHECKED);

                if (enableShell)
                {
                    FileAssociation::RegisterShellMenu();
                }
                else
                {
                    FileAssociation::UnregisterShellMenu();
                }

                AppSettings::Instance().SetOpenFolderAfterExtract(IsDlgButtonChecked(hDlg, IDC_CHK_DEF_OPEN_FOLDER) == BST_CHECKED);

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

