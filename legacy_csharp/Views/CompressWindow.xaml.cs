using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.IO;
using System.Linq;
using System.Windows;
using Microsoft.Win32;

namespace EasyZip.Views
{
    public partial class CompressWindow : Window
    {
        public ObservableCollection<string> SourcePaths { get; } = new();
        public string DestinationArchive { get; private set; } = string.Empty;
        public System.IO.Compression.CompressionLevel SelectedLevel { get; private set; } = System.IO.Compression.CompressionLevel.Optimal;

        public CompressWindow(IEnumerable<string>? initialSources = null)
        {
            InitializeComponent();
            ListSources.ItemsSource = SourcePaths;

            if (initialSources != null)
            {
                foreach (var s in initialSources.Where(p => File.Exists(p) || Directory.Exists(p)))
                {
                    if (!SourcePaths.Contains(s))
                    {
                        SourcePaths.Add(s);
                    }
                }
            }

            UpdateUiState();
            AutoSuggestDestinationPath();
        }

        private void UpdateUiState()
        {
            TxtSourcesInfo.Text = $"{SourcePaths.Count}개 항목";
            BtnStart.IsEnabled = SourcePaths.Count > 0;
        }

        private void AutoSuggestDestinationPath()
        {
            if (SourcePaths.Count == 0) return;
            if (!string.IsNullOrEmpty(TxtArchivePath.Text.Trim())) return;

            string first = SourcePaths[0];
            string parentDir = Path.GetDirectoryName(first) ?? "C:\\";
            string baseName = Path.GetFileNameWithoutExtension(first);
            if (string.IsNullOrEmpty(baseName)) baseName = "Archive";

            TxtArchivePath.Text = Path.Combine(parentDir, $"{baseName}.zip");
        }

        private void BtnAddFiles_Click(object sender, RoutedEventArgs e)
        {
            var ofd = new OpenFileDialog
            {
                Title = "압축할 파일 추가",
                Multiselect = true,
                Filter = "모든 파일 (*.*)|*.*"
            };

            if (ofd.ShowDialog(this) == true)
            {
                foreach (var f in ofd.FileNames)
                {
                    if (!SourcePaths.Contains(f))
                    {
                        SourcePaths.Add(f);
                    }
                }
                UpdateUiState();
                AutoSuggestDestinationPath();
            }
        }

        private void BtnAddFolder_Click(object sender, RoutedEventArgs e)
        {
            var ofd = new OpenFolderDialog
            {
                Title = "압축할 폴더 선택",
                Multiselect = true
            };

            if (ofd.ShowDialog(this) == true)
            {
                foreach (var f in ofd.FolderNames)
                {
                    if (!SourcePaths.Contains(f))
                    {
                        SourcePaths.Add(f);
                    }
                }
                UpdateUiState();
                AutoSuggestDestinationPath();
            }
        }

        private void BtnRemoveSelected_Click(object sender, RoutedEventArgs e)
        {
            var selected = ListSources.SelectedItems.Cast<string>().ToList();
            foreach (var item in selected)
            {
                SourcePaths.Remove(item);
            }
            UpdateUiState();
        }

        private void BtnClearAll_Click(object sender, RoutedEventArgs e)
        {
            SourcePaths.Clear();
            UpdateUiState();
        }

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

        private void Window_Drop(object sender, DragEventArgs e)
        {
            if (e.Data.GetDataPresent(DataFormats.FileDrop))
            {
                string[] files = (string[])e.Data.GetData(DataFormats.FileDrop);
                foreach (var f in files)
                {
                    if ((File.Exists(f) || Directory.Exists(f)) && !SourcePaths.Contains(f))
                    {
                        SourcePaths.Add(f);
                    }
                }
                UpdateUiState();
                AutoSuggestDestinationPath();
            }
        }

        private void BtnBrowse_Click(object sender, RoutedEventArgs e)
        {
            var sfd = new SaveFileDialog
            {
                Title = "압축 파일 저장 경로 지정",
                Filter = "ZIP 아카이브 (*.zip)|*.zip|모든 파일 (*.*)|*.*",
                FileName = Path.GetFileName(TxtArchivePath.Text)
            };

            if (sfd.ShowDialog(this) == true)
            {
                TxtArchivePath.Text = sfd.FileName;
            }
        }

        private void BtnStart_Click(object sender, RoutedEventArgs e)
        {
            if (SourcePaths.Count == 0)
            {
                MessageBox.Show("압축할 파일이나 폴더를 추가해주세요.", "알림", MessageBoxButton.OK, MessageBoxImage.Warning);
                return;
            }

            string dest = TxtArchivePath.Text.Trim();
            if (string.IsNullOrEmpty(dest))
            {
                MessageBox.Show("저장할 압축 파일 경로를 입력하세요.", "알림", MessageBoxButton.OK, MessageBoxImage.Warning);
                return;
            }

            if (!dest.EndsWith(".zip", StringComparison.OrdinalIgnoreCase))
            {
                dest += ".zip";
            }

            DestinationArchive = dest;

            SelectedLevel = CmbLevel.SelectedIndex switch
            {
                0 => System.IO.Compression.CompressionLevel.Fastest,
                1 => System.IO.Compression.CompressionLevel.Optimal,
                2 => System.IO.Compression.CompressionLevel.SmallestSize,
                3 => System.IO.Compression.CompressionLevel.NoCompression,
                _ => System.IO.Compression.CompressionLevel.Optimal
            };

            DialogResult = true;
            Close();
        }

        private void BtnCancel_Click(object sender, RoutedEventArgs e)
        {
            DialogResult = false;
            Close();
        }
    }
}
