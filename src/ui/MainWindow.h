#pragma once

#include <windows.h>
#include <commctrl.h>
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include "res/resource.h"
#include "ArchiveEngine.h"

namespace EasyZip
{
    struct TreeNodeData
    {
        std::wstring folderPath; // relative folder path (e.g. "docs/reports")
        std::wstring name;       // display name (e.g. "reports")
    };

    class MainWindow
    {
    public:
        static MainWindow* Create();
        ~MainWindow();

        bool Initialize(HINSTANCE hInstance, int nCmdShow);
        HWND GetHwnd() const { return m_hWnd; }

        void OpenArchive(const std::wstring& archivePath);
        void ExtractCurrentArchive(bool selectedOnly = false);
        void CompressFiles(const std::vector<std::wstring>& files);
        void TestCurrentArchive();

    private:
        MainWindow();

        static LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
        static LRESULT CALLBACK SplitterProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

        void CreateControls();
        void UpdateLayout(int width, int height);
        void BuildFolderTree();
        void PopulateListView(const std::wstring& folderPath);
        void NavigateToFolder(const std::wstring& folderPath);
        void CollapseAllTreeNodes();
        void UpdateStatusBar();
        void SetCodepage(const std::string& codepage, UINT menuId);
        void UpdateCodepageMenuCheck();

        HWND m_hWnd = NULL;
        HINSTANCE m_hInstance = NULL;

        HWND m_hToolbar = NULL;
        HWND m_hAddressBar = NULL;
        HWND m_hBtnUp = NULL;
        HWND m_hSplitter = NULL;
        HWND m_hTreeView = NULL;
        HWND m_hListView = NULL;
        HWND m_hStatusBar = NULL;

        HFONT m_hFont = NULL;
        HIMAGELIST m_hToolbarImageList = NULL;

        int m_splitterPos = 240;
        bool m_bDraggingSplitter = false;

        std::wstring m_currentArchivePath;
        std::wstring m_currentFolder = L""; // current directory path inside archive
        std::string m_currentCodepage = ""; // Empty means Auto (system default CP949)
        UINT m_activeCodepageMenuId = IDM_CODEPAGE_AUTO;
        std::vector<ArchiveEntry> m_entries;
        std::unordered_map<HTREEITEM, TreeNodeData> m_treeMap;
    };
}

