using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Threading.Tasks;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Input;
using Microsoft.Win32;
using EasyZip.Models;
using EasyZip.Services;

namespace EasyZip.Views
{
    public partial class MainWindow : Window
    {
        public enum ViewMode
        {
            Home,
            Archive
        }

        private ViewMode _currentMode = ViewMode.Home;

        // 아카이브 탐색 상태
        private string _currentArchivePath = string.Empty;
        private string _currentArchiveSubPath = string.Empty;
        private List<ArchiveItem> _allArchiveItems = new();

        // 좌측 아카이브 폴더 트리 상태
        private ArchiveFolderNode? _archiveRootNode;
        private bool _isSyncingTreeSelection;
        private double _lastFolderTreeWidth = 220;

        // 서브폴더 이동 히스토리
        private readonly Stack<string> _backHistory = new();
        private readonly Stack<string> _forwardHistory = new();
        private bool _isNavigatingHistory;

        public MainWindow(string? initialPath = null)
        {
            InitializeComponent();

            Loaded += async (s, e) =>
            {
                if (!string.IsNullOrEmpty(initialPath))
                {
                    await OpenPathAsync(initialPath);
                }
                else
                {
                    SetViewMode(ViewMode.Home);
                }
            };
        }

        #region 뷰 모드 전환 (홈 / 압축 파일 내부)

        public void SetViewMode(ViewMode mode)
        {
            _currentMode = mode;

            switch (mode)
            {
                case ViewMode.Home:
                    PanelHome.Visibility = Visibility.Visible;
                    PanelArchive.Visibility = Visibility.Collapsed;

                    Title = "EasyZip - 초고속 압축 관리자";
                    TxtCurrentPath.Text = "";
                    BtnToolbarClose.IsEnabled = false;
                    BtnToolbarExtract.IsEnabled = false;
                    TxtStatusLeft.Text = "준비 완료 - 압축 파일을 드래그하거나 열기를 클릭하세요.";
                    break;

                case ViewMode.Archive:
                    PanelHome.Visibility = Visibility.Collapsed;
                    PanelArchive.Visibility = Visibility.Visible;

                    Title = $"{Path.GetFileName(_currentArchivePath)} - EasyZip";
                    BtnToolbarClose.IsEnabled = true;
                    BtnToolbarExtract.IsEnabled = true;
                    break;
            }
        }

        #endregion

        #region 통합 경로 오픈

        public async Task OpenPathAsync(string path)
        {
            if (File.Exists(path))
            {
                if (ArchiveEngine.IsSupportedArchive(path))
                {
                    await OpenArchiveAsync(path);
                    return;
                }
            }

            SetViewMode(ViewMode.Home);
        }

        #endregion

        #region 압축 파일 내부 탐색 모드 (핵심 기능)

        public async Task OpenArchiveAsync(string archivePath, string subPath = "")
        {
            try
            {
                TxtStatusLeft.Text = $"압축 파일 분석 중: {Path.GetFileName(archivePath)}...";
                _allArchiveItems = await ArchiveEngine.GetArchiveEntriesAsync(archivePath);

                _currentArchivePath = archivePath;
                _currentArchiveSubPath = subPath.Trim('/', '\\').Replace('\\', '/');

                _backHistory.Clear();
                _forwardHistory.Clear();

                BuildArchiveFolderTree();

                SetViewMode(ViewMode.Archive);
                DisplayArchiveSubPath();
            }
            catch (Exception ex)
            {
                string diagnosis = ArchiveEngine.DiagnoseArchiveError(archivePath, ex);
                MessageBox.Show($"압축 파일을 열 수 없습니다:\n\n{diagnosis}", "EasyZip 안내", MessageBoxButton.OK, MessageBoxImage.Warning);
                if (string.IsNullOrEmpty(_currentArchivePath))
                {
                    SetViewMode(ViewMode.Home);
                }
            }
        }

        private void DisplayArchiveSubPath()
        {
            string display = _currentArchivePath;
            if (!string.IsNullOrEmpty(_currentArchiveSubPath))
            {
                display += " > " + _currentArchiveSubPath;
            }
            TxtCurrentPath.Text = display;

            var currentLevelItems = new List<ArchiveItem>();

            // 상위 폴더 항목
            if (!string.IsNullOrEmpty(_currentArchiveSubPath))
            {
                currentLevelItems.Add(new ArchiveItem
                {
                    Name = "..",
                    FullPath = "..",
                    IsDirectory = true
                });
            }

            var subDirNames = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
            string prefix = string.IsNullOrEmpty(_currentArchiveSubPath) ? "" : _currentArchiveSubPath + "/";

            foreach (var item in _allArchiveItems)
            {
                string rel = item.FullPath;
                if (!rel.StartsWith(prefix, StringComparison.OrdinalIgnoreCase))
                    continue;

                string remainder = rel.Substring(prefix.Length).TrimStart('/');
                if (string.IsNullOrEmpty(remainder)) continue;

                int slashIndex = remainder.IndexOf('/');
                if (slashIndex >= 0)
                {
                    // 하위 폴더
                    string folderName = remainder.Substring(0, slashIndex);
                    if (subDirNames.Add(folderName))
                    {
                        currentLevelItems.Add(new ArchiveItem
                        {
                            Name = folderName,
                            FullPath = prefix + folderName,
                            IsDirectory = true
                        });
                    }
                }
                else
                {
                    // 현재 레벨 파일
                    currentLevelItems.Add(item);
                }
            }

            // 폴더 먼저, 그 다음 파일 정렬
            var sorted = currentLevelItems
                .OrderByDescending(i => i.Name == "..")
                .ThenByDescending(i => i.IsDirectory)
                .ThenBy(i => i.Name)
                .ToList();

            string filter = TxtSearch?.Text.Trim() ?? "";
            if (!string.IsNullOrEmpty(filter))
            {
                sorted = _allArchiveItems
                    .Where(i => i.Name.Contains(filter, StringComparison.OrdinalIgnoreCase) || i.FullPath.Contains(filter, StringComparison.OrdinalIgnoreCase))
                    .OrderByDescending(i => i.IsDirectory)
                    .ThenBy(i => i.Name)
                    .ToList();
            }

            ListArchiveItems.ItemsSource = sorted;

            int fileCount = _allArchiveItems.Count(i => !i.IsDirectory);
            long totalUncompressed = _allArchiveItems.Where(i => !i.IsDirectory).Sum(i => i.Size);
            TxtStatusLeft.Text = $"압축 아카이브 열림: {Path.GetFileName(_currentArchivePath)} (총 {fileCount}개 파일, {ArchiveItem.FormatBytes(totalUncompressed)})";

            SyncTreeSelection(_currentArchiveSubPath);
        }

        #region 압축 파일 내부 폴더 트리 뷰 (좌측 패널)

        private void BuildArchiveFolderTree()
        {
            if (string.IsNullOrEmpty(_currentArchivePath) || _allArchiveItems == null)
            {
                TreeArchiveFolders.ItemsSource = null;
                _archiveRootNode = null;
                return;
            }

            string rootName = Path.GetFileName(_currentArchivePath);
            var root = new ArchiveFolderNode
            {
                Name = rootName,
                FullPath = "",
                Icon = "🗜️",
                IsExpanded = true,
                IsSelected = string.IsNullOrEmpty(_currentArchiveSubPath)
            };

            // 1. 모든 고유 폴더 경로 수집
            var allFolderPaths = new HashSet<string>(StringComparer.OrdinalIgnoreCase);

            foreach (var item in _allArchiveItems)
            {
                string path = item.FullPath.Replace('\\', '/').Trim('/');
                if (string.IsNullOrEmpty(path)) continue;

                if (item.IsDirectory)
                {
                    allFolderPaths.Add(path);
                }
                else
                {
                    int lastSlash = path.LastIndexOf('/');
                    if (lastSlash > 0)
                    {
                        string dir = path.Substring(0, lastSlash);
                        allFolderPaths.Add(dir);
                    }
                }
            }

            // 2. 상위 경로들도 모두 포함되도록 보장 (예: a/b/c 가 있으면 a, a/b 도 포함)
            var expandedFolders = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
            foreach (var folder in allFolderPaths)
            {
                string[] parts = folder.Split('/', StringSplitOptions.RemoveEmptyEntries);
                string current = "";
                foreach (var p in parts)
                {
                    current = string.IsNullOrEmpty(current) ? p : current + "/" + p;
                    expandedFolders.Add(current);
                }
            }

            // 3. 트리 계층 구조 구축
            var nodeLookup = new Dictionary<string, ArchiveFolderNode>(StringComparer.OrdinalIgnoreCase)
            {
                [""] = root
            };

            var sortedFolders = expandedFolders
                .OrderBy(f => f.Count(c => c == '/'))
                .ThenBy(f => f, StringComparer.OrdinalIgnoreCase)
                .ToList();
            foreach (var folderPath in sortedFolders)
            {
                int lastSlash = folderPath.LastIndexOf('/');
                string parentPath = lastSlash >= 0 ? folderPath.Substring(0, lastSlash) : "";
                string folderName = lastSlash >= 0 ? folderPath.Substring(lastSlash + 1) : folderPath;

                if (!nodeLookup.TryGetValue(parentPath, out var parentNode))
                {
                    parentNode = root;
                }

                var folderNode = new ArchiveFolderNode
                {
                    Name = folderName,
                    FullPath = folderPath,
                    Icon = "📁",
                    IsExpanded = false, // 기본적으로 모든 하위 폴더는 닫힌(접힌) 상태에서 시작
                    IsSelected = string.Equals(_currentArchiveSubPath, folderPath, StringComparison.OrdinalIgnoreCase)
                };

                nodeLookup[folderPath] = folderNode;
                parentNode.Children.Add(folderNode);
            }

            _archiveRootNode = root;
            TreeArchiveFolders.ItemsSource = new[] { root };
        }

        private void TreeArchiveFolders_SelectedItemChanged(object sender, RoutedPropertyChangedEventArgs<object> e)
        {
            if (_isSyncingTreeSelection) return;
            if (e.NewValue is ArchiveFolderNode node)
            {
                if (!string.Equals(_currentArchiveSubPath, node.FullPath, StringComparison.OrdinalIgnoreCase))
                {
                    NavigateToArchiveSubPath(node.FullPath);
                }
            }
        }

        private void SyncTreeSelection(string targetSubPath)
        {
            if (_archiveRootNode == null) return;

            _isSyncingTreeSelection = true;
            try
            {
                SelectNodeRecursive(_archiveRootNode, targetSubPath);
            }
            finally
            {
                _isSyncingTreeSelection = false;
            }
        }

        private bool SelectNodeRecursive(ArchiveFolderNode current, string targetSubPath)
        {
            bool isMatch = string.Equals(current.FullPath, targetSubPath, StringComparison.OrdinalIgnoreCase);
            if (isMatch)
            {
                current.IsSelected = true;
                return true;
            }

            current.IsSelected = false;
            bool childMatched = false;
            foreach (var child in current.Children)
            {
                if (SelectNodeRecursive(child, targetSubPath))
                {
                    current.IsExpanded = true;
                    childMatched = true;
                }
            }

            return childMatched;
        }

        private void BtnCollapseAll_Click(object sender, RoutedEventArgs e)
        {
            if (_archiveRootNode == null) return;
            CollapseRecursive(_archiveRootNode);
            _archiveRootNode.IsExpanded = true; // 아카이브 최상위 루트만 펼쳐두기
        }

        private void CollapseRecursive(ArchiveFolderNode node)
        {
            node.IsExpanded = false;
            foreach (var child in node.Children)
            {
                CollapseRecursive(child);
            }
        }

        private void BtnToggleTree_Click(object sender, RoutedEventArgs e)
        {
            if (ColFolderTree.Width.Value > 0)
            {
                _lastFolderTreeWidth = ColFolderTree.Width.Value;
                ColFolderTree.Width = new GridLength(0);
                ColFolderTree.MinWidth = 0;
            }
            else
            {
                ColFolderTree.MinWidth = 130;
                ColFolderTree.Width = new GridLength(_lastFolderTreeWidth > 130 ? _lastFolderTreeWidth : 220);
            }
        }

        private void BtnRefreshTree_Click(object sender, RoutedEventArgs e)
        {
            BuildArchiveFolderTree();
            SyncTreeSelection(_currentArchiveSubPath);
        }

        #endregion

        private void TxtSearch_TextChanged(object sender, TextChangedEventArgs e)
        {
            if (TxtSearchPlaceholder != null)
            {
                TxtSearchPlaceholder.Visibility = string.IsNullOrEmpty(TxtSearch.Text) ? Visibility.Visible : Visibility.Collapsed;
            }
            if (_currentMode == ViewMode.Archive)
            {
                DisplayArchiveSubPath();
            }
        }

        private void NavigateToArchiveSubPath(string newSubPath, bool recordHistory = true)
        {
            if (recordHistory && !_isNavigatingHistory)
            {
                _backHistory.Push(_currentArchiveSubPath);
                _forwardHistory.Clear();
            }

            _currentArchiveSubPath = newSubPath.Trim('/', '\\').Replace('\\', '/');
            DisplayArchiveSubPath();
        }

        #endregion

        #region 툴바 및 네비게이션 버튼 핸들러

        private async void BtnOpenArchive_Click(object sender, RoutedEventArgs e)
        {
            var ofd = new OpenFileDialog
            {
                Title = "압축 파일 열기",
                Filter = "모든 지원 압축 파일 (*.zip;*.7z;*.rar;*.tar;*.gz;*.bz2;*.xz;*.iso;*.cab;*.alz;*.egg;*.001)|*.zip;*.7z;*.rar;*.tar;*.gz;*.tgz;*.bz2;*.tbz2;*.xz;*.txz;*.iso;*.cab;*.alz;*.egg;*.001|분할 압축 파일 (*.001)|*.001|알집 파일 (*.alz;*.egg)|*.alz;*.egg|ZIP 압축 파일 (*.zip)|*.zip|7-Zip 아카이브 (*.7z)|*.7z|모든 파일 (*.*)|*.*"
            };

            if (ofd.ShowDialog() == true)
            {
                await OpenArchiveAsync(ofd.FileName);
            }
        }

        private void BtnCloseArchive_Click(object sender, RoutedEventArgs e)
        {
            _currentArchivePath = string.Empty;
            _currentArchiveSubPath = string.Empty;
            _allArchiveItems.Clear();
            _backHistory.Clear();
            _forwardHistory.Clear();
            TreeArchiveFolders.ItemsSource = null;
            _archiveRootNode = null;
            SetViewMode(ViewMode.Home);
        }

        private void BtnExtract_Click(object sender, RoutedEventArgs e)
        {
            string archiveToExtract = _currentArchivePath;

            if (string.IsNullOrEmpty(archiveToExtract) || !File.Exists(archiveToExtract))
            {
                BtnOpenArchive_Click(sender, e);
                return;
            }

            // 기본 대상 폴더: 압축 파일과 동일한 위치에 파일명으로 폴더 생성 (폴더 중복 시 (2), (3) 자동 증가)
            string parent = Path.GetDirectoryName(archiveToExtract) ?? "C:\\";
            string folderName = Path.GetFileNameWithoutExtension(archiveToExtract);
            string defaultDest = ArchiveEngine.GetUniqueDirectoryPath(parent, folderName);

            var progressWnd = new ProgressWindow(archiveToExtract, defaultDest) { Owner = this };
            progressWnd.ShowDialog();
        }

        private void BtnCompress_Click(object sender, RoutedEventArgs e)
        {
            var compWnd = new CompressWindow() { Owner = this };
            if (compWnd.ShowDialog() == true && compWnd.SourcePaths.Count > 0)
            {
                var progressWnd = new ProgressWindow(compWnd.SourcePaths, compWnd.DestinationArchive, compWnd.SelectedLevel)
                {
                    Owner = this
                };
                progressWnd.ShowDialog();
            }
        }

        private void BtnUp_Click(object sender, RoutedEventArgs e)
        {
            if (_currentMode == ViewMode.Archive)
            {
                if (!string.IsNullOrEmpty(_currentArchiveSubPath))
                {
                    int lastSlash = _currentArchiveSubPath.LastIndexOf('/');
                    string parent = lastSlash >= 0 ? _currentArchiveSubPath.Substring(0, lastSlash) : "";
                    NavigateToArchiveSubPath(parent);
                }
            }
        }

        private void BtnRefresh_Click(object sender, RoutedEventArgs e)
        {
            if (_currentMode == ViewMode.Archive && !string.IsNullOrEmpty(_currentArchivePath))
            {
                _ = OpenArchiveAsync(_currentArchivePath, _currentArchiveSubPath);
            }
        }

        private void BtnSettings_Click(object sender, RoutedEventArgs e)
        {
            var settingsWnd = new SettingsWindow { Owner = this };
            settingsWnd.ShowDialog();
        }

        private void BtnBack_Click(object sender, RoutedEventArgs e)
        {
            if (_currentMode == ViewMode.Archive && _backHistory.Count > 0)
            {
                _forwardHistory.Push(_currentArchiveSubPath);
                _isNavigatingHistory = true;
                string prev = _backHistory.Pop();
                NavigateToArchiveSubPath(prev, false);
                _isNavigatingHistory = false;
            }
        }

        private void BtnForward_Click(object sender, RoutedEventArgs e)
        {
            if (_currentMode == ViewMode.Archive && _forwardHistory.Count > 0)
            {
                _backHistory.Push(_currentArchiveSubPath);
                _isNavigatingHistory = true;
                string next = _forwardHistory.Pop();
                NavigateToArchiveSubPath(next, false);
                _isNavigatingHistory = false;
            }
        }

        private void TxtCurrentPath_KeyDown(object sender, KeyEventArgs e)
        {
            if (e.Key == Key.Enter)
            {
                BtnGo_Click(sender, e);
            }
        }

        private async void BtnGo_Click(object sender, RoutedEventArgs e)
        {
            string path = TxtCurrentPath.Text.Trim();
            if (string.IsNullOrEmpty(path)) return;

            if (File.Exists(path))
            {
                await OpenPathAsync(path);
            }
        }

        #endregion

        #region 리스트 더블클릭 및 선택 이벤트

        private async void ListArchiveItems_MouseDoubleClick(object sender, MouseButtonEventArgs e)
        {
            if (ListArchiveItems.SelectedItem is ArchiveItem item)
            {
                if (item.Name == "..")
                {
                    BtnUp_Click(sender, e);
                }
                else if (item.IsDirectory)
                {
                    NavigateToArchiveSubPath(item.FullPath);
                }
                else
                {
                    // 내부 파일 더블클릭 -> 임시 폴더에 초고속 추출 후 기본 앱으로 바로 열기!
                    try
                    {
                        TxtStatusLeft.Text = $"임시 추출 및 실행 중: {item.Name}...";
                        string extracted = await ArchiveEngine.ExtractSingleFileAsync(_currentArchivePath, item.FullPath);
                        Process.Start(new ProcessStartInfo { FileName = extracted, UseShellExecute = true });
                        TxtStatusLeft.Text = $"실행 완료: {item.Name}";
                    }
                    catch (Exception ex)
                    {
                        MessageBox.Show($"파일을 열 수 없습니다:\n{ex.Message}", "EasyZip", MessageBoxButton.OK, MessageBoxImage.Error);
                    }
                }
            }
        }

        private void ListArchiveItems_SelectionChanged(object sender, SelectionChangedEventArgs e)
        {
            var selected = ListArchiveItems.SelectedItems.Cast<ArchiveItem>().Where(i => i.Name != "..").ToList();
            if (selected.Count > 0)
            {
                long bytes = selected.Sum(i => i.Size);
                TxtStatusLeft.Text = $"{selected.Count}개 파일 선택됨 (원본 합계: {ArchiveItem.FormatBytes(bytes)})";
            }
        }

        #endregion

        #region 컨텍스트 메뉴 액션

        private async void MenuArchiveOpen_Click(object sender, RoutedEventArgs e)
        {
            if (ListArchiveItems.SelectedItem is ArchiveItem item && !item.IsDirectory)
            {
                try
                {
                    string extracted = await ArchiveEngine.ExtractSingleFileAsync(_currentArchivePath, item.FullPath);
                    Process.Start(new ProcessStartInfo { FileName = extracted, UseShellExecute = true });
                }
                catch (Exception ex)
                {
                    MessageBox.Show($"파일을 열 수 없습니다:\n{ex.Message}", "EasyZip", MessageBoxButton.OK, MessageBoxImage.Error);
                }
            }
        }

        private async void MenuArchiveExtractSelected_Click(object sender, RoutedEventArgs e)
        {
            var selected = ListArchiveItems.SelectedItems.Cast<ArchiveItem>().Where(i => !i.IsDirectory && i.Name != "..").ToList();
            if (selected.Count == 0)
            {
                BtnExtract_Click(sender, e);
                return;
            }

            var dialog = new OpenFolderDialog
            {
                Title = "선택한 항목을 압축 풀 대상 폴더 선택",
                InitialDirectory = Path.GetDirectoryName(_currentArchivePath) ?? "C:\\"
            };

            if (dialog.ShowDialog() == true)
            {
                string targetDir = dialog.FolderName;
                try
                {
                    int done = 0;
                    foreach (var item in selected)
                    {
                        TxtStatusLeft.Text = $"선택 항목 추출 중 ({++done}/{selected.Count}): {item.Name}";
                        string temp = await ArchiveEngine.ExtractSingleFileAsync(_currentArchivePath, item.FullPath);
                        string destFile = Path.Combine(targetDir, Path.GetFileName(item.FullPath));
                        File.Copy(temp, destFile, true);
                    }
                    TxtStatusLeft.Text = $"선택한 {selected.Count}개 파일 압축 해제 완료: {targetDir}";
                    Process.Start("explorer.exe", targetDir);
                }
                catch (Exception ex)
                {
                    MessageBox.Show($"추출 중 오류 발생:\n{ex.Message}", "EasyZip", MessageBoxButton.OK, MessageBoxImage.Error);
                }
            }
        }

        #endregion

        #region 드래그 앤 드롭 지원

        private void Window_DragOver(object sender, DragEventArgs e)
        {
            if (e.Data.GetDataPresent(DataFormats.FileDrop))
            {
                e.Effects = DragDropEffects.Copy;
            }
            else
            {
                e.Effects = DragDropEffects.None;
            }
            e.Handled = true;
        }

        private async void Window_Drop(object sender, DragEventArgs e)
        {
            if (e.Data.GetDataPresent(DataFormats.FileDrop))
            {
                string[] files = (string[])e.Data.GetData(DataFormats.FileDrop);
                if (files.Length == 1 && ArchiveEngine.IsSupportedArchive(files[0]))
                {
                    await OpenArchiveAsync(files[0]);
                }
                else if (files.Length > 0)
                {
                    // 압축 대화상자 오픈
                    var compWnd = new CompressWindow(files) { Owner = this };
                    if (compWnd.ShowDialog() == true)
                    {
                        var pWnd = new ProgressWindow(compWnd.SourcePaths, compWnd.DestinationArchive, compWnd.SelectedLevel) { Owner = this };
                        pWnd.ShowDialog();
                    }
                }
            }
        }

        #endregion
    }
}
