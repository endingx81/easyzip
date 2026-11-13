#include "MainWindow.h"
#include "PathHelper.h"
#include "SystemIconHelper.h"
#include "ProgressDlg.h"
#include "CompressDlg.h"
#include "SettingsDlg.h"
#include "AppSettings.h"
#include "FileAssociation.h"
#include "res/resource.h"
#include <commctrl.h>
#include <commdlg.h>
#include <shellapi.h>
#include <shlwapi.h>
#include <shlobj.h>
#include <shobjidl.h>
#include <algorithm>
#include <set>

#pragma comment(lib, "comctl32.lib")

namespace EasyZip
{
    static const wchar_t* MAIN_WINDOW_CLASS = L"EasyZip_MainWindowClass";
    static const wchar_t* SPLITTER_CLASS = L"EasyZip_SplitterClass";

    MainWindow* MainWindow::Create()
    {
        return new MainWindow();
    }

    MainWindow::MainWindow()
    {
    }

    MainWindow::~MainWindow()
    {
        if (m_hFont)
        {
            DeleteObject(m_hFont);
            m_hFont = NULL;
        }
        if (m_hToolbarImageList)
        {
            ImageList_Destroy(m_hToolbarImageList);
            m_hToolbarImageList = NULL;
        }
    }

    bool MainWindow::Initialize(HINSTANCE hInstance, int nCmdShow)
    {
        m_hInstance = hInstance;

        INITCOMMONCONTROLSEX icex = { 0 };
        icex.dwSize = sizeof(icex);
        icex.dwICC = ICC_WIN95_CLASSES | ICC_COOL_CLASSES | ICC_BAR_CLASSES;
        InitCommonControlsEx(&icex);

        // Register main window class
        WNDCLASSEXW wc = { 0 };
        wc.cbSize = sizeof(wc);
        wc.style = CS_HREDRAW | CS_VREDRAW;
        wc.lpfnWndProc = WndProc;
        wc.hInstance = hInstance;
        wc.hIcon = LoadIconW(hInstance, MAKEINTRESOURCEW(IDI_APP_ICON));
        wc.hIconSm = LoadIconW(hInstance, MAKEINTRESOURCEW(IDI_APP_ICON));
        wc.hCursor = LoadCursorW(NULL, IDC_ARROW);
        wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
        wc.lpszMenuName = MAKEINTRESOURCEW(IDR_MAIN_MENU);
        wc.lpszClassName = MAIN_WINDOW_CLASS;
        RegisterClassExW(&wc);

        // Register splitter class
        WNDCLASSEXW sc = { 0 };
        sc.cbSize = sizeof(sc);
        sc.lpfnWndProc = SplitterProc;
        sc.hInstance = hInstance;
        sc.hCursor = LoadCursorW(NULL, IDC_SIZEWE);
        sc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_BTNFACE + 1);
        sc.lpszClassName = SPLITTER_CLASS;
        RegisterClassExW(&sc);

        // Standard UI font
        m_hFont = CreateFontW(-12, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"맑은 고딕");

        m_hWnd = CreateWindowExW(
            WS_EX_ACCEPTFILES,
            MAIN_WINDOW_CLASS,
            L"EasyZip",
            WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
            CW_USEDEFAULT, CW_USEDEFAULT, 980, 640,
            NULL, NULL, hInstance, this);

        if (!m_hWnd) return false;

        ShowWindow(m_hWnd, nCmdShow);
        UpdateWindow(m_hWnd);
        UpdateCodepageMenuCheck();

        // First run check: if not done yet and .zip is not registered to EasyZip, prompt settings dialog
        if (!AppSettings::Instance().GetFirstRunDone())
        {
            FileAssociation::RegisterShellMenu();
            if (!FileAssociation::IsExtensionRegistered(L".zip"))
            {
                // Register base capabilities & ProgIds so Windows 11 recognizes EasyZip
                FileAssociation::RegisterAll();
                // Show Settings Dialog on File Association tab
                SettingsDlg::Show(m_hWnd);
            }
            AppSettings::Instance().SetFirstRunDone(true);
        }

        return true;
    }

    void MainWindow::CreateControls()
    {
        // 1. Toolbar
        m_hToolbar = CreateWindowExW(
            0, TOOLBARCLASSNAMEW, NULL,
            WS_CHILD | WS_VISIBLE | TBSTYLE_FLAT | TBSTYLE_TOOLTIPS | CCS_TOP | CCS_NODIVIDER,
            0, 0, 0, 0, m_hWnd, reinterpret_cast<HMENU>(IDC_TOOLBAR), m_hInstance, NULL);

        SendMessageW(m_hToolbar, TB_BUTTONSTRUCTSIZE, sizeof(TBBUTTON), 0);

        UINT dpi = GetDpiForWindow(m_hWnd);
        int iconSize = MulDiv(16, dpi, 96);
        if (iconSize < 16) iconSize = 16;
        SendMessageW(m_hToolbar, TB_SETBITMAPSIZE, 0, MAKELPARAM(iconSize, iconSize));

        // Create dedicated toolbar image list with clear functional icons
        m_hToolbarImageList = ImageList_Create(iconSize, iconSize, ILC_COLOR32 | ILC_MASK, 2, 1);

        // 0. 압축 풀기 (Extract)
        HICON hIconExtract = NULL;
        ExtractIconExW(L"imageres.dll", 165, NULL, &hIconExtract, 1);
        if (!hIconExtract) ExtractIconExW(L"shell32.dll", 45, NULL, &hIconExtract, 1);
        if (hIconExtract) { ImageList_AddIcon(m_hToolbarImageList, hIconExtract); DestroyIcon(hIconExtract); }

        // 1. 설정 (Settings / Gear)
        HICON hIconSettings = NULL;
        ExtractIconExW(L"imageres.dll", 63, NULL, &hIconSettings, 1);
        if (!hIconSettings) ExtractIconExW(L"shell32.dll", 21, NULL, &hIconSettings, 1);
        if (hIconSettings) { ImageList_AddIcon(m_hToolbarImageList, hIconSettings); DestroyIcon(hIconSettings); }

        SendMessageW(m_hToolbar, TB_SETIMAGELIST, 0, reinterpret_cast<LPARAM>(m_hToolbarImageList));

        TBBUTTON tbButtons[] = {
            { 0, IDM_FILE_EXTRACT_ALL,      TBSTATE_ENABLED, BTNS_BUTTON | BTNS_AUTOSIZE, {0}, 0, (INT_PTR)L" 압축 풀기 " },
            { 0, 0,                         0,               BTNS_SEP,                   {0}, 0, 0 },
            { 1, IDM_TOOLS_SETTINGS,        TBSTATE_ENABLED, BTNS_BUTTON | BTNS_AUTOSIZE, {0}, 0, (INT_PTR)L" 설정 " }
        };
        SendMessageW(m_hToolbar, TB_ADDBUTTONSW, _countof(tbButtons), reinterpret_cast<LPARAM>(tbButtons));

        // 2. Navigation / Address Bar
        m_hBtnUp = CreateWindowExW(
            0, L"Button", L"⇪ 상위",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            0, 0, 50, 24, m_hWnd, reinterpret_cast<HMENU>(IDC_BTN_UP_FOLDER), m_hInstance, NULL);
        SendMessageW(m_hBtnUp, WM_SETFONT, reinterpret_cast<WPARAM>(m_hFont), TRUE);

        m_hAddressBar = CreateWindowExW(
            WS_EX_CLIENTEDGE, L"Edit", L"/",
            WS_CHILD | WS_VISIBLE | ES_READONLY | ES_AUTOHSCROLL,
            0, 0, 100, 24, m_hWnd, reinterpret_cast<HMENU>(IDC_ADDRESS_BAR), m_hInstance, NULL);
        SendMessageW(m_hAddressBar, WM_SETFONT, reinterpret_cast<WPARAM>(m_hFont), TRUE);

        HIMAGELIST hSysIcons = SystemIconHelper::Instance().GetSystemImageListSmall();

        // 3. TreeView (Left folder panel)
        m_hTreeView = CreateWindowExW(
            WS_EX_CLIENTEDGE, WC_TREEVIEWW, NULL,
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | TVS_HASLINES | TVS_LINESATROOT | TVS_HASBUTTONS | TVS_SHOWSELALWAYS,
            0, 0, 240, 100, m_hWnd, reinterpret_cast<HMENU>(IDC_TREEVIEW), m_hInstance, NULL);
        TreeView_SetImageList(m_hTreeView, hSysIcons, TVSIL_NORMAL);
        SendMessageW(m_hTreeView, WM_SETFONT, reinterpret_cast<WPARAM>(m_hFont), TRUE);

        // 4. Splitter
        m_hSplitter = CreateWindowExW(
            0, SPLITTER_CLASS, NULL,
            WS_CHILD | WS_VISIBLE,
            240, 0, 5, 100, m_hWnd, NULL, m_hInstance, NULL);
        SetWindowLongPtrW(m_hSplitter, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));

        // 5. ListView (Right file list)
        m_hListView = CreateWindowExW(
            WS_EX_CLIENTEDGE, WC_LISTVIEWW, NULL,
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | LVS_REPORT | LVS_SHOWSELALWAYS | LVS_SHAREIMAGELISTS,
            245, 0, 500, 100, m_hWnd, reinterpret_cast<HMENU>(IDC_LISTVIEW), m_hInstance, NULL);
        ListView_SetExtendedListViewStyle(m_hListView, LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER | LVS_EX_GRIDLINES);
        ListView_SetImageList(m_hListView, hSysIcons, LVSIL_SMALL);
        SendMessageW(m_hListView, WM_SETFONT, reinterpret_cast<WPARAM>(m_hFont), TRUE);

        // Add Columns
        LVCOLUMNW lvc = { 0 };
        lvc.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM | LVCF_FMT;

        lvc.fmt = LVCFMT_LEFT;
        lvc.cx = 260;
        lvc.pszText = const_cast<LPWSTR>(L"이름");
        ListView_InsertColumn(m_hListView, 0, &lvc);

        lvc.fmt = LVCFMT_RIGHT;
        lvc.cx = 100;
        lvc.pszText = const_cast<LPWSTR>(L"크기");
        ListView_InsertColumn(m_hListView, 1, &lvc);

        lvc.fmt = LVCFMT_RIGHT;
        lvc.cx = 100;
        lvc.pszText = const_cast<LPWSTR>(L"압축 크기");
        ListView_InsertColumn(m_hListView, 2, &lvc);

        lvc.fmt = LVCFMT_CENTER;
        lvc.cx = 140;
        lvc.pszText = const_cast<LPWSTR>(L"수정한 날짜");
        ListView_InsertColumn(m_hListView, 3, &lvc);

        lvc.fmt = LVCFMT_LEFT;
        lvc.cx = 120;
        lvc.pszText = const_cast<LPWSTR>(L"종류");
        ListView_InsertColumn(m_hListView, 4, &lvc);

        lvc.fmt = LVCFMT_LEFT;
        lvc.cx = 220;
        lvc.pszText = const_cast<LPWSTR>(L"경로");
        ListView_InsertColumn(m_hListView, 5, &lvc);

        // 6. Status Bar
        m_hStatusBar = CreateWindowExW(
            0, STATUSCLASSNAMEW, NULL,
            WS_CHILD | WS_VISIBLE | SBARS_SIZEGRIP,
            0, 0, 0, 0, m_hWnd, reinterpret_cast<HMENU>(IDC_STATUSBAR), m_hInstance, NULL);
        SendMessageW(m_hStatusBar, WM_SETFONT, reinterpret_cast<WPARAM>(m_hFont), TRUE);

        int sbParts[] = { 400, 700, -1 };
        SendMessageW(m_hStatusBar, SB_SETPARTS, _countof(sbParts), reinterpret_cast<LPARAM>(sbParts));
        SendMessageW(m_hStatusBar, SB_SETTEXTW, 0, reinterpret_cast<LPARAM>(L"준비"));
    }

    void MainWindow::UpdateLayout(int width, int height)
    {
        if (width <= 0 || height <= 0) return;

        SendMessageW(m_hToolbar, TB_AUTOSIZE, 0, 0);
        RECT rcToolbar;
        GetWindowRect(m_hToolbar, &rcToolbar);
        int toolbarHeight = rcToolbar.bottom - rcToolbar.top;

        RECT rcStatus;
        GetWindowRect(m_hStatusBar, &rcStatus);
        int statusHeight = rcStatus.bottom - rcStatus.top;

        // Navigation bar
        int navY = toolbarHeight + 2;
        int navH = 26;
        int btnUpW = 55;
        MoveWindow(m_hBtnUp, 6, navY, btnUpW, navH, TRUE);
        
        int addrX = 6 + btnUpW + 6;
        int addrW = width - addrX - 6;
        if (addrW < 100) addrW = 100;
        MoveWindow(m_hAddressBar, addrX, navY, addrW, navH, TRUE);

        // Content Area (TreeView + Splitter + ListView)
        int contentY = navY + navH + 4;
        int contentH = height - contentY - statusHeight;
        if (contentH < 50) contentH = 50;

        // Bound splitter position
        if (m_splitterPos < 120) m_splitterPos = 120;
        if (m_splitterPos > width - 150) m_splitterPos = width - 150;

        int splitterWidth = 5;
        MoveWindow(m_hTreeView, 0, contentY, m_splitterPos, contentH, TRUE);
        MoveWindow(m_hSplitter, m_splitterPos, contentY, splitterWidth, contentH, TRUE);

        int listX = m_splitterPos + splitterWidth;
        int listW = width - listX;
        if (listW < 50) listW = 50;
        MoveWindow(m_hListView, listX, contentY, listW, contentH, TRUE);

        SendMessageW(m_hStatusBar, WM_SIZE, 0, 0);
    }

    void MainWindow::OpenArchive(const std::wstring& archivePath)
    {
        if (!PathHelper::FileExists(archivePath)) return;

        m_currentArchivePath = archivePath;
        std::wstring err;
        if (!ArchiveEngine::ReadEntries(archivePath, m_entries, err, m_currentCodepage))
        {
            MessageBoxW(m_hWnd, (L"압축 파일을 열 수 없습니다:\n" + err).c_str(), L"오류", MB_ICONERROR);
            return;
        }

        // Set title
        std::wstring title = PathHelper::GetFileName(archivePath) + L" - EasyZip";
        SetWindowTextW(m_hWnd, title.c_str());

        // Rebuild folder tree & populate list
        m_currentFolder.clear();
        BuildFolderTree();
        PopulateListView(L"");
        UpdateStatusBar();
        UpdateCodepageMenuCheck();
    }

    void MainWindow::BuildFolderTree()
    {
        SendMessageW(m_hTreeView, WM_SETREDRAW, FALSE, 0);
        TreeView_DeleteAllItems(m_hTreeView);
        m_treeMap.clear();

        if (m_currentArchivePath.empty())
        {
            SendMessageW(m_hTreeView, WM_SETREDRAW, TRUE, 0);
            return;
        }

        int folderIcon = SystemIconHelper::Instance().GetFolderIconIndex(false);
        int folderOpenIcon = SystemIconHelper::Instance().GetFolderIconIndex(true);
        int archiveIcon = SystemIconHelper::Instance().GetArchiveIconIndex(m_currentArchivePath);

        // Add Root Node (Archive Name)
        TVINSERTSTRUCTW tvis = { 0 };
        tvis.hParent = TVI_ROOT;
        tvis.hInsertAfter = TVI_LAST;
        tvis.item.mask = TVIF_TEXT | TVIF_IMAGE | TVIF_SELECTEDIMAGE | TVIF_PARAM;
        tvis.item.iImage = archiveIcon;
        tvis.item.iSelectedImage = archiveIcon;

        std::wstring rootName = PathHelper::GetFileName(m_currentArchivePath);
        tvis.item.pszText = const_cast<LPWSTR>(rootName.c_str());
        HTREEITEM hRoot = TreeView_InsertItem(m_hTreeView, &tvis);

        m_treeMap[hRoot] = { L"", rootName };

        // Collect all distinct folder paths
        std::set<std::wstring> allFolders;
        for (const auto& e : m_entries)
        {
            std::wstring norm = PathHelper::Normalize(e.fullPath);
            if (e.isDirectory)
            {
                allFolders.insert(norm);
            }
            else
            {
                std::wstring dir = PathHelper::GetDirectory(norm);
                while (!dir.empty())
                {
                    allFolders.insert(dir);
                    dir = PathHelper::GetDirectory(dir);
                }
            }
        }

        // Mapping from folder path to HTREEITEM
        std::unordered_map<std::wstring, HTREEITEM> folderToItem;
        folderToItem[L""] = hRoot;

        for (const auto& folderPath : allFolders)
        {
            if (folderPath.empty()) continue;

            // Split into parts to ensure parent hierarchy
            std::wstring current;
            HTREEITEM hParent = hRoot;

            size_t start = 0;
            while (start < folderPath.length())
            {
                size_t slash = folderPath.find(L'\\', start);
                std::wstring part = (slash == std::wstring::npos) ? folderPath.substr(start) : folderPath.substr(start, slash - start);
                current = current.empty() ? part : (current + L"\\" + part);

                auto it = folderToItem.find(current);
                if (it == folderToItem.end())
                {
                    TVINSERTSTRUCTW sub = { 0 };
                    sub.hParent = hParent;
                    sub.hInsertAfter = TVI_LAST;
                    sub.item.mask = TVIF_TEXT | TVIF_IMAGE | TVIF_SELECTEDIMAGE;
                    sub.item.iImage = folderIcon;
                    sub.item.iSelectedImage = folderOpenIcon;
                    sub.item.pszText = const_cast<LPWSTR>(part.c_str());

                    HTREEITEM hSub = TreeView_InsertItem(m_hTreeView, &sub);
                    folderToItem[current] = hSub;
                    m_treeMap[hSub] = { current, part };
                    hParent = hSub;
                }
                else
                {
                    hParent = it->second;
                }

                if (slash == std::wstring::npos) break;
                start = slash + 1;
            }
        }

        // CRITICAL REQUIREMENT: Root and all nodes start COLLAPSED by default!
        TreeView_Expand(m_hTreeView, hRoot, TVE_COLLAPSE);
        TreeView_SelectItem(m_hTreeView, hRoot);

        SendMessageW(m_hTreeView, WM_SETREDRAW, TRUE, 0);
        InvalidateRect(m_hTreeView, NULL, TRUE);
    }

    void MainWindow::CollapseAllTreeNodes()
    {
        HTREEITEM hRoot = TreeView_GetRoot(m_hTreeView);
        if (!hRoot) return;

        auto collapseRecursive = [this](auto& self, HTREEITEM hItem) -> void {
            HTREEITEM hChild = TreeView_GetChild(m_hTreeView, hItem);
            while (hChild)
            {
                self(self, hChild);
                hChild = TreeView_GetNextSibling(m_hTreeView, hChild);
            }
            TreeView_Expand(m_hTreeView, hItem, TVE_COLLAPSE);
        };

        SendMessageW(m_hTreeView, WM_SETREDRAW, FALSE, 0);
        collapseRecursive(collapseRecursive, hRoot);
        SendMessageW(m_hTreeView, WM_SETREDRAW, TRUE, 0);
        InvalidateRect(m_hTreeView, NULL, TRUE);
    }

    void MainWindow::PopulateListView(const std::wstring& folderPath)
    {
        m_currentFolder = PathHelper::Normalize(folderPath);
        while (!m_currentFolder.empty() && m_currentFolder.back() == L'\\') m_currentFolder.pop_back();

        // Update address bar
        std::wstring displayAddr = m_currentFolder.empty() ? L"/" : (L"/" + m_currentFolder + L"/");
        std::replace(displayAddr.begin(), displayAddr.end(), L'\\', L'/');
        SetWindowTextW(m_hAddressBar, displayAddr.c_str());

        SendMessageW(m_hListView, WM_SETREDRAW, FALSE, 0);
        ListView_DeleteAllItems(m_hListView);

        // 1. Add ".." entry if not at root
        if (!m_currentFolder.empty())
        {
            LVITEMW lvi = { 0 };
            lvi.mask = LVIF_TEXT | LVIF_IMAGE | LVIF_PARAM;
            lvi.iItem = 0;
            lvi.iSubItem = 0;
            lvi.pszText = const_cast<LPWSTR>(L"..");
            lvi.iImage = SystemIconHelper::Instance().GetFolderIconIndex(false);
            lvi.lParam = -1; // -1 indicates parent navigation
            ListView_InsertItem(m_hListView, &lvi);
            ListView_SetItemText(m_hListView, 0, 4, const_cast<LPWSTR>(L"상위 폴더"));
        }

        // 2. Identify immediate children in this folder
        std::set<std::wstring> subfolders;
        std::vector<const ArchiveEntry*> directFiles;

        for (const auto& e : m_entries)
        {
            std::wstring itemPath = PathHelper::Normalize(e.fullPath);
            std::wstring itemDir = PathHelper::GetDirectory(itemPath);

            if (itemDir == m_currentFolder)
            {
                if (e.isDirectory)
                {
                    subfolders.insert(PathHelper::GetFileName(itemPath));
                }
                else
                {
                    directFiles.push_back(&e);
                }
            }
            else if (!m_currentFolder.empty() && itemPath.rfind(m_currentFolder + L"\\", 0) == 0)
            {
                // Subdirectory further down
                std::wstring rel = itemPath.substr(m_currentFolder.length() + 1);
                size_t slash = rel.find(L'\\');
                if (slash != std::wstring::npos)
                {
                    subfolders.insert(rel.substr(0, slash));
                }
            }
            else if (m_currentFolder.empty() && !itemDir.empty())
            {
                // Root level: first segment of path is a subfolder
                size_t slash = itemPath.find(L'\\');
                if (slash != std::wstring::npos)
                {
                    subfolders.insert(itemPath.substr(0, slash));
                }
            }
        }

        int index = ListView_GetItemCount(m_hListView);

        // 3. Add subfolders
        int folderIcon = SystemIconHelper::Instance().GetFolderIconIndex(false);
        for (const auto& f : subfolders)
        {
            LVITEMW lvi = { 0 };
            lvi.mask = LVIF_TEXT | LVIF_IMAGE | LVIF_PARAM;
            lvi.iItem = index;
            lvi.iSubItem = 0;
            lvi.pszText = const_cast<LPWSTR>(f.c_str());
            lvi.iImage = folderIcon;
            lvi.lParam = -2; // -2 indicates folder
            ListView_InsertItem(m_hListView, &lvi);
            ListView_SetItemText(m_hListView, index, 4, const_cast<LPWSTR>(L"파일 폴더"));
            index++;
        }

        // 4. Add files
        for (const auto* pFile : directFiles)
        {
            LVITEMW lvi = { 0 };
            lvi.mask = LVIF_TEXT | LVIF_IMAGE | LVIF_PARAM;
            lvi.iItem = index;
            lvi.iSubItem = 0;
            lvi.pszText = const_cast<LPWSTR>(pFile->name.c_str());
            lvi.iImage = SystemIconHelper::Instance().GetIconIndexForPath(pFile->name, false);
            lvi.lParam = reinterpret_cast<LPARAM>(pFile);
            ListView_InsertItem(m_hListView, &lvi);

            // Size
            std::wstring szStr = PathHelper::FormatFileSize(pFile->uncompressedSize);
            ListView_SetItemText(m_hListView, index, 1, const_cast<LPWSTR>(szStr.c_str()));

            // Compressed Size
            std::wstring cszStr = (pFile->compressedSize > 0) ? PathHelper::FormatFileSize(pFile->compressedSize) : L"-";
            ListView_SetItemText(m_hListView, index, 2, const_cast<LPWSTR>(cszStr.c_str()));

            // Modified Time
            ListView_SetItemText(m_hListView, index, 3, const_cast<LPWSTR>(pFile->lastModifiedStr.c_str()));

            // Type
            std::wstring ext = PathHelper::GetExtension(pFile->name);
            std::wstring typeStr = ext.empty() ? L"파일" : (ext.substr(1) + L" 파일");
            ListView_SetItemText(m_hListView, index, 4, const_cast<LPWSTR>(typeStr.c_str()));

            // Full relative path
            ListView_SetItemText(m_hListView, index, 5, const_cast<LPWSTR>(pFile->fullPath.c_str()));

            index++;
        }

        SendMessageW(m_hListView, WM_SETREDRAW, TRUE, 0);
        InvalidateRect(m_hListView, NULL, TRUE);

        UpdateStatusBar();
    }

    void MainWindow::NavigateToFolder(const std::wstring& folderPath)
    {
        PopulateListView(folderPath);

        // Sync TreeView selection
        for (const auto& pair : m_treeMap)
        {
            if (pair.second.folderPath == m_currentFolder)
            {
                TreeView_SelectItem(m_hTreeView, pair.first);
                break;
            }
        }
    }

    void MainWindow::UpdateStatusBar()
    {
        if (m_currentArchivePath.empty())
        {
            SendMessageW(m_hStatusBar, SB_SETTEXTW, 0, reinterpret_cast<LPARAM>(L"준비"));
            SendMessageW(m_hStatusBar, SB_SETTEXTW, 1, reinterpret_cast<LPARAM>(L""));
            SendMessageW(m_hStatusBar, SB_SETTEXTW, 2, reinterpret_cast<LPARAM>(L""));
            return;
        }

        SendMessageW(m_hStatusBar, SB_SETTEXTW, 0, reinterpret_cast<LPARAM>(m_currentArchivePath.c_str()));

        uint64_t totalBytes = 0;
        uint32_t totalFiles = 0;
        for (const auto& e : m_entries)
        {
            if (!e.isDirectory)
            {
                totalBytes += e.uncompressedSize;
                totalFiles++;
            }
        }

        std::wstring summary = std::to_wstring(totalFiles) + L"개 파일 (" + PathHelper::FormatFileSize(totalBytes) + L")";
        SendMessageW(m_hStatusBar, SB_SETTEXTW, 1, reinterpret_cast<LPARAM>(summary.c_str()));

        int selCount = ListView_GetSelectedCount(m_hListView);
        std::wstring selStr = (selCount > 0) ? (std::to_wstring(selCount) + L"개 항목 선택됨") : L"";
        SendMessageW(m_hStatusBar, SB_SETTEXTW, 2, reinterpret_cast<LPARAM>(selStr.c_str()));
    }

    void MainWindow::SetCodepage(const std::string& codepage, UINT menuId)
    {
        m_currentCodepage = codepage;
        m_activeCodepageMenuId = menuId;
        UpdateCodepageMenuCheck();

        if (!m_currentArchivePath.empty())
        {
            OpenArchive(m_currentArchivePath);
        }
    }

    void MainWindow::UpdateCodepageMenuCheck()
    {
        HMENU hMenu = GetMenu(m_hWnd);
        if (hMenu)
        {
            CheckMenuRadioItem(hMenu, IDM_CODEPAGE_AUTO, IDM_CODEPAGE_CP1252, m_activeCodepageMenuId, MF_BYCOMMAND);
        }
    }

    static bool BrowseForFolder(HWND hWndParent, const std::wstring& initialDir, const std::wstring& title, std::wstring& outFolder)
    {
        HRESULT hrInit = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);

        bool bSuccess = false;
        IFileOpenDialog* pDlg = nullptr;
        HRESULT hr = CoCreateInstance(CLSID_FileOpenDialog, NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pDlg));
        if (SUCCEEDED(hr))
        {
            DWORD dwOptions = 0;
            if (SUCCEEDED(pDlg->GetOptions(&dwOptions)))
            {
                pDlg->SetOptions(dwOptions | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM | FOS_PATHMUSTEXIST);
            }
            if (!title.empty())
            {
                pDlg->SetTitle(title.c_str());
            }
            if (!initialDir.empty())
            {
                IShellItem* pItem = nullptr;
                if (SUCCEEDED(SHCreateItemFromParsingName(initialDir.c_str(), NULL, IID_PPV_ARGS(&pItem))))
                {
                    pDlg->SetFolder(pItem);
                    pItem->Release();
                }
            }

            hr = pDlg->Show(hWndParent);
            if (SUCCEEDED(hr))
            {
                IShellItem* pResult = nullptr;
                if (SUCCEEDED(pDlg->GetResult(&pResult)))
                {
                    PWSTR pszPath = nullptr;
                    if (SUCCEEDED(pResult->GetDisplayName(SIGDN_FILESYSPATH, &pszPath)))
                    {
                        outFolder = pszPath;
                        CoTaskMemFree(pszPath);
                        bSuccess = true;
                    }
                    pResult->Release();
                }
            }
            pDlg->Release();
        }
        else
        {
            // Fallback: SHBrowseForFolderW
            BROWSEINFOW bi = { 0 };
            bi.hwndOwner = hWndParent;
            bi.lpszTitle = title.c_str();
            bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE | BIF_USENEWUI;
            PIDLIST_ABSOLUTE pidl = SHBrowseForFolderW(&bi);
            if (pidl)
            {
                wchar_t path[MAX_PATH] = { 0 };
                if (SHGetPathFromIDListW(pidl, path))
                {
                    outFolder = path;
                    bSuccess = true;
                }
                CoTaskMemFree(pidl);
            }
        }

        if (SUCCEEDED(hrInit))
        {
            CoUninitialize();
        }

        return bSuccess;
    }

    void MainWindow::ExtractCurrentArchive(bool selectedOnly)
    {
        if (m_currentArchivePath.empty()) return;

        // 1. 대상 폴더 지정 창 팝업 (사용자가 풀 위치를 직접 지정)
        std::wstring initialDir = AppSettings::Instance().GetLastExtractDir();
        if (initialDir.empty() || !PathHelper::DirectoryExists(initialDir))
        {
            initialDir = PathHelper::GetDirectory(m_currentArchivePath);
        }

        std::wstring targetDir;
        if (!BrowseForFolder(m_hWnd, initialDir, selectedOnly ? L"선택한 항목을 압축 풀 대상 폴더를 선택하십시오" : L"압축을 풀 대상 폴더를 선택하십시오", targetDir))
        {
            return; // 사용자가 취소(Cancel/ESC)한 경우 아무것도 하지 않고 안전하게 중단
        }

        // 대상 폴더 기억
        AppSettings::Instance().SetLastExtractDir(targetDir);

        // Filter target paths if selectedOnly
        std::vector<std::wstring> targets;
        if (selectedOnly)
        {
            int idx = -1;
            while ((idx = ListView_GetNextItem(m_hListView, idx, LVNI_SELECTED)) != -1)
            {
                LVITEMW lvi = { 0 };
                lvi.mask = LVIF_PARAM;
                lvi.iItem = idx;
                ListView_GetItem(m_hListView, &lvi);

                if (lvi.lParam == -1) continue; // ".."

                wchar_t pathBuf[MAX_PATH * 2] = { 0 };
                ListView_GetItemText(m_hListView, idx, 5, pathBuf, _countof(pathBuf));
                if (wcslen(pathBuf) > 0)
                {
                    targets.push_back(pathBuf);
                }
                else
                {
                    wchar_t nameBuf[MAX_PATH * 2] = { 0 };
                    ListView_GetItemText(m_hListView, idx, 0, nameBuf, _countof(nameBuf));
                    std::wstring fullSub = m_currentFolder.empty() ? nameBuf : (m_currentFolder + L"\\" + nameBuf);
                    targets.push_back(fullSub);
                }
            }
        }

        ProgressDlg progress(m_hWnd, L"압축 푸는 중 - " + PathHelper::GetFileName(m_currentArchivePath));
        progress.SetDestinationFolder(targetDir);
        progress.Show();

        std::wstring errMsg;
        bool ok = false;
        if (targets.empty())
        {
            ok = ArchiveEngine::ExtractAll(
                m_currentArchivePath,
                targetDir,
                [&](const ProgressInfo& info) { progress.UpdateProgress(info); },
                [&]() { return progress.IsCanceled(); },
                &errMsg,
                m_currentCodepage);
        }
        else
        {
            ok = ArchiveEngine::ExtractSelected(
                m_currentArchivePath,
                targetDir,
                targets,
                [&](const ProgressInfo& info) { progress.UpdateProgress(info); },
                [&]() { return progress.IsCanceled(); },
                &errMsg,
                m_currentCodepage);
        }

        progress.Close();

        if (ok && AppSettings::Instance().GetOpenFolderAfterExtract())
        {
            ShellExecuteW(m_hWnd, L"open", targetDir.c_str(), NULL, NULL, SW_SHOWNORMAL);
        }

        if (!ok && !progress.IsCanceled())
        {
            MessageBoxW(m_hWnd, (L"압축 풀기 실패:\n" + errMsg).c_str(), L"오류", MB_ICONERROR);
        }
    }

    void MainWindow::CompressFiles(const std::vector<std::wstring>& files)
    {
        if (files.empty()) return;

        CompressOptions opts;
        if (!CompressDlg::Show(m_hWnd, files, opts))
        {
            return;
        }

        ProgressDlg progress(m_hWnd, L"압축하는 중 - " + PathHelper::GetFileName(opts.destArchivePath));
        progress.Show();

        std::wstring errMsg;
        bool ok = ArchiveEngine::Compress(
            opts.sourcePaths,
            opts.destArchivePath,
            opts.format,
            opts.level,
            [&](const ProgressInfo& info) { progress.UpdateProgress(info); },
            [&]() { return progress.IsCanceled(); },
            &errMsg);

        progress.Close();

        if (ok)
        {
            OpenArchive(opts.destArchivePath);
        }
        else if (!progress.IsCanceled())
        {
            MessageBoxW(m_hWnd, (L"압축 생성 실패:\n" + errMsg).c_str(), L"오류", MB_ICONERROR);
        }
    }

    void MainWindow::TestCurrentArchive()
    {
        if (m_currentArchivePath.empty()) return;

        ProgressDlg progress(m_hWnd, L"아카이브 무결성 테스트 - " + PathHelper::GetFileName(m_currentArchivePath));
        progress.Show();

        std::wstring errMsg;
        bool ok = ArchiveEngine::TestArchive(
            m_currentArchivePath,
            [&](const ProgressInfo& info) { progress.UpdateProgress(info); },
            [&]() { return progress.IsCanceled(); },
            &errMsg,
            m_currentCodepage);

        progress.Close();

        if (ok)
        {
            MessageBoxW(m_hWnd, L"아카이브에 오류가 없으며 무결성이 확인되었습니다.", L"테스트 성공", MB_ICONINFORMATION);
        }
        else if (!progress.IsCanceled())
        {
            MessageBoxW(m_hWnd, (L"아카이브에서 오류가 발견되었습니다:\n" + errMsg).c_str(), L"오류", MB_ICONERROR);
        }
    }

    LRESULT CALLBACK MainWindow::SplitterProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
    {
        MainWindow* pMain = reinterpret_cast<MainWindow*>(GetWindowLongPtrW(hWnd, GWLP_USERDATA));

        switch (msg)
        {
        case WM_LBUTTONDOWN:
            if (pMain)
            {
                pMain->m_bDraggingSplitter = true;
                SetCapture(hWnd);
            }
            return 0;

        case WM_MOUSEMOVE:
            if (pMain && pMain->m_bDraggingSplitter)
            {
                POINT pt;
                GetCursorPos(&pt);
                ScreenToClient(pMain->m_hWnd, &pt);

                pMain->m_splitterPos = pt.x;
                RECT rc;
                GetClientRect(pMain->m_hWnd, &rc);
                pMain->UpdateLayout(rc.right, rc.bottom);
            }
            return 0;

        case WM_LBUTTONUP:
            if (pMain && pMain->m_bDraggingSplitter)
            {
                pMain->m_bDraggingSplitter = false;
                ReleaseCapture();
            }
            return 0;
        }

        return DefWindowProcW(hWnd, msg, wParam, lParam);
    }

    LRESULT CALLBACK MainWindow::WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
    {
        MainWindow* pThis = reinterpret_cast<MainWindow*>(GetWindowLongPtrW(hWnd, GWLP_USERDATA));

        switch (msg)
        {
        case WM_CREATE:
        {
            CREATESTRUCTW* pcs = reinterpret_cast<CREATESTRUCTW*>(lParam);
            pThis = reinterpret_cast<MainWindow*>(pcs->lpCreateParams);
            pThis->m_hWnd = hWnd;
            SetWindowLongPtrW(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pThis));
            pThis->CreateControls();
            return 0;
        }

        case WM_SIZE:
            if (pThis)
            {
                pThis->UpdateLayout(LOWORD(lParam), HIWORD(lParam));
            }
            return 0;

        case WM_GETMINMAXINFO:
        {
            MINMAXINFO* pMMI = reinterpret_cast<MINMAXINFO*>(lParam);
            pMMI->ptMinTrackSize.x = 640;
            pMMI->ptMinTrackSize.y = 440;
            return 0;
        }

        case WM_DROPFILES:
            if (pThis)
            {
                HDROP hDrop = reinterpret_cast<HDROP>(wParam);
                UINT fileCount = DragQueryFileW(hDrop, 0xFFFFFFFF, NULL, 0);
                std::vector<std::wstring> dropped;
                for (UINT i = 0; i < fileCount; ++i)
                {
                    wchar_t buf[MAX_PATH * 2] = { 0 };
                    DragQueryFileW(hDrop, i, buf, _countof(buf));
                    dropped.push_back(buf);
                }
                DragFinish(hDrop);

                if (dropped.size() == 1 && ArchiveEngine::IsSupportedArchive(dropped[0]))
                {
                    pThis->OpenArchive(dropped[0]);
                }
                else if (!dropped.empty())
                {
                    pThis->CompressFiles(dropped);
                }
            }
            return 0;

        case WM_NOTIFY:
            if (pThis)
            {
                NMHDR* pnm = reinterpret_cast<NMHDR*>(lParam);

                // TreeView item selected
                if (pnm->idFrom == IDC_TREEVIEW && pnm->code == TVN_SELCHANGEDW)
                {
                    NMTREEVIEWW* pnmtv = reinterpret_cast<NMTREEVIEWW*>(lParam);
                    HTREEITEM hSel = pnmtv->itemNew.hItem;
                    auto it = pThis->m_treeMap.find(hSel);
                    if (it != pThis->m_treeMap.end())
                    {
                        pThis->PopulateListView(it->second.folderPath);
                    }
                    return 0;
                }

                // ListView double click
                if (pnm->idFrom == IDC_LISTVIEW && pnm->code == NM_DBLCLK)
                {
                    NMITEMACTIVATE* pnmia = reinterpret_cast<NMITEMACTIVATE*>(lParam);
                    if (pnmia->iItem >= 0)
                    {
                        LVITEMW lvi = { 0 };
                        lvi.mask = LVIF_PARAM;
                        lvi.iItem = pnmia->iItem;
                        ListView_GetItem(pThis->m_hListView, &lvi);

                        if (lvi.lParam == -1) // ".." parent directory
                        {
                            std::wstring parent = PathHelper::GetDirectory(pThis->m_currentFolder);
                            pThis->NavigateToFolder(parent);
                        }
                        else if (lvi.lParam == -2) // Subfolder
                        {
                            wchar_t folderName[MAX_PATH] = { 0 };
                            ListView_GetItemText(pThis->m_hListView, pnmia->iItem, 0, folderName, _countof(folderName));
                            std::wstring target = pThis->m_currentFolder.empty() ? folderName : (pThis->m_currentFolder + L"\\" + folderName);
                            pThis->NavigateToFolder(target);
                        }
                    }
                    return 0;
                }

                // ListView selection changed
                if (pnm->idFrom == IDC_LISTVIEW && pnm->code == LVN_ITEMCHANGED)
                {
                    pThis->UpdateStatusBar();
                    return 0;
                }
            }
            break;

        case WM_COMMAND:
            if (pThis)
            {
                switch (LOWORD(wParam))
                {
                case IDM_FILE_OPEN:
                {
                    wchar_t fileBuf[MAX_PATH * 2] = { 0 };
                    OPENFILENAMEW ofn = { 0 };
                    ofn.lStructSize = sizeof(ofn);
                    ofn.hwndOwner = hWnd;
                    ofn.lpstrFilter = L"모든 지원 아카이브 (*.zip;*.7z;*.rar;*.alz;*.egg;*.tar;*.gz;*.bz2;*.xz;*.iso)\0*.zip;*.7z;*.rar;*.alz;*.egg;*.tar;*.gz;*.tgz;*.bz2;*.tbz2;*.xz;*.txz;*.iso;*.cab;*.001\0"
                                      L"ZIP 아카이브 (*.zip)\0*.zip\0"
                                      L"7-Zip 아카이브 (*.7z)\0*.7z\0"
                                      L"RAR 아카이브 (*.rar)\0*.rar\0"
                                      L"알집 아카이브 (*.alz;*.egg)\0*.alz;*.egg\0"
                                      L"모든 파일 (*.*)\0*.*\0";
                    ofn.lpstrFile = fileBuf;
                    ofn.nMaxFile = _countof(fileBuf);
                    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;

                    if (GetOpenFileNameW(&ofn))
                    {
                        pThis->OpenArchive(fileBuf);
                    }
                    return 0;
                }

                case IDM_FILE_COMPRESS:
                {
                    // Open file dialog for source files
                    wchar_t multiBuf[65536] = { 0 };
                    OPENFILENAMEW ofn = { 0 };
                    ofn.lStructSize = sizeof(ofn);
                    ofn.hwndOwner = hWnd;
                    ofn.lpstrFilter = L"모든 파일 (*.*)\0*.*\0";
                    ofn.lpstrFile = multiBuf;
                    ofn.nMaxFile = _countof(multiBuf);
                    ofn.Flags = OFN_FILEMUSTEXIST | OFN_ALLOWMULTISELECT | OFN_EXPLORER;

                    if (GetOpenFileNameW(&ofn))
                    {
                        std::vector<std::wstring> sources;
                        const wchar_t* p = multiBuf;
                        std::wstring first = p;
                        p += first.length() + 1;

                        if (*p == L'\0')
                        {
                            sources.push_back(first);
                        }
                        else
                        {
                            while (*p)
                            {
                                std::wstring fName = p;
                                sources.push_back(PathHelper::Combine(first, fName));
                                p += fName.length() + 1;
                            }
                        }

                        pThis->CompressFiles(sources);
                    }
                    return 0;
                }

                case IDM_FILE_EXTRACT_ALL:
                    pThis->ExtractCurrentArchive(false);
                    return 0;

                case IDM_FILE_EXTRACT_SELECTED:
                    pThis->ExtractCurrentArchive(true);
                    return 0;

                case IDM_TOOLS_TEST:
                    pThis->TestCurrentArchive();
                    return 0;

                case IDM_TOOLS_SETTINGS:
                    SettingsDlg::Show(hWnd);
                    return 0;

                case IDM_CODEPAGE_AUTO:
                    pThis->SetCodepage("", IDM_CODEPAGE_AUTO);
                    return 0;

                case IDM_CODEPAGE_CP949:
                    pThis->SetCodepage("CP949", IDM_CODEPAGE_CP949);
                    return 0;

                case IDM_CODEPAGE_UTF8:
                    pThis->SetCodepage("UTF-8", IDM_CODEPAGE_UTF8);
                    return 0;

                case IDM_CODEPAGE_CP932:
                    pThis->SetCodepage("CP932", IDM_CODEPAGE_CP932);
                    return 0;

                case IDM_CODEPAGE_CP936:
                    pThis->SetCodepage("CP936", IDM_CODEPAGE_CP936);
                    return 0;

                case IDM_CODEPAGE_CP950:
                    pThis->SetCodepage("CP950", IDM_CODEPAGE_CP950);
                    return 0;

                case IDM_CODEPAGE_CP1252:
                    pThis->SetCodepage("CP1252", IDM_CODEPAGE_CP1252);
                    return 0;

                case IDM_HELP_ABOUT:
                    DialogBoxW(GetModuleHandleW(NULL), MAKEINTRESOURCEW(IDD_ABOUT), hWnd,
                        [](HWND hDlg, UINT m, WPARAM w, LPARAM) -> INT_PTR {
                            if (m == WM_COMMAND && (LOWORD(w) == IDOK || LOWORD(w) == IDCANCEL)) {
                                EndDialog(hDlg, IDOK);
                                return TRUE;
                            }
                            return FALSE;
                        });
                    return 0;

                case IDC_BTN_UP_FOLDER:
                case IDM_VIEW_UP_FOLDER:
                    if (!pThis->m_currentFolder.empty())
                    {
                        std::wstring parent = PathHelper::GetDirectory(pThis->m_currentFolder);
                        pThis->NavigateToFolder(parent);
                    }
                    return 0;

                case IDC_BTN_COLLAPSE_ALL:
                case IDM_VIEW_COLLAPSE_ALL:
                    pThis->CollapseAllTreeNodes();
                    return 0;

                case IDM_VIEW_REFRESH:
                    if (!pThis->m_currentArchivePath.empty())
                    {
                        pThis->OpenArchive(pThis->m_currentArchivePath);
                    }
                    return 0;

                case IDM_FILE_CLOSE:
                    pThis->m_currentArchivePath.clear();
                    pThis->m_entries.clear();
                    pThis->m_currentFolder.clear();
                    pThis->BuildFolderTree();
                    pThis->PopulateListView(L"");
                    SetWindowTextW(hWnd, L"EasyZip");
                    return 0;

                case IDM_FILE_EXIT:
                    PostMessageW(hWnd, WM_CLOSE, 0, 0);
                    return 0;
                }
            }
            break;

        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
        }

        return DefWindowProcW(hWnd, msg, wParam, lParam);
    }
}
