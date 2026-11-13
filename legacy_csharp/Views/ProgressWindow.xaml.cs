using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Threading;
using System.Threading.Tasks;
using System.Windows;
using EasyZip.Models;
using EasyZip.Services;

namespace EasyZip.Views
{
    public partial class ProgressWindow : Window
    {
        private readonly CancellationTokenSource _cts = new();
        public bool IsExtraction => _isExtraction;
        private readonly bool _isExtraction;
        private readonly string? _archivePath;
        private readonly string _destination;
        private readonly IEnumerable<string>? _sourcePaths;
        private readonly System.IO.Compression.CompressionLevel _compressionLevel;

        public bool IsSuccess { get; private set; }

        /// <summary>
        /// 압축 풀기 모드 생성자
        /// </summary>
        public ProgressWindow(string archivePath, string destinationDirectory)
        {
            InitializeComponent();
            _isExtraction = true;
            _archivePath = archivePath;
            _destination = destinationDirectory;

            var settings = AppSettings.Load();
            ChkOpenFolder.IsChecked = settings.OpenFolderAfterExtraction;
            HookSettingPersistence();

            TxtTitle.Text = $"압축 풀기: {Path.GetFileName(archivePath)}";
            Loaded += async (s, e) => await StartExtractionAsync();
        }

        /// <summary>
        /// 압축하기 모드 생성자
        /// </summary>
        public ProgressWindow(IEnumerable<string> sourcePaths, string destinationArchive, System.IO.Compression.CompressionLevel level)
        {
            InitializeComponent();
            _isExtraction = false;
            _sourcePaths = sourcePaths;
            _destination = destinationArchive;
            _compressionLevel = level;

            var settings = AppSettings.Load();
            ChkOpenFolder.IsChecked = settings.OpenFolderAfterCompression;
            HookSettingPersistence();

            TxtTitle.Text = $"초고속 압축 생성: {Path.GetFileName(destinationArchive)}";
            Loaded += async (s, e) => await StartCompressionAsync();
        }

        private void HookSettingPersistence()
        {
            ChkOpenFolder.Click += (s, e) => SaveOpenFolderPreference();
        }

        private void SaveOpenFolderPreference()
        {
            try
            {
                var settings = AppSettings.Load();
                if (_isExtraction)
                {
                    settings.OpenFolderAfterExtraction = ChkOpenFolder.IsChecked == true;
                }
                else
                {
                    settings.OpenFolderAfterCompression = ChkOpenFolder.IsChecked == true;
                }
                settings.Save();
            }
            catch { }
        }

        private async Task StartExtractionAsync()
        {
            var progress = new Progress<ExtractionProgressInfo>(UpdateUi);

            try
            {
                await ArchiveEngine.ExtractArchiveAsync(_archivePath!, _destination, progress, _cts.Token);
                IsSuccess = true;

                if (ChkOpenFolder.IsChecked == true && Directory.Exists(_destination))
                {
                    Process.Start(new ProcessStartInfo
                    {
                        FileName = _destination,
                        UseShellExecute = true
                    });
                }

                await Task.Delay(400); // 사용자에게 완료 체감 피드백 후 종료
                Close();
            }
            catch (OperationCanceledException)
            {
                MessageBox.Show("작업이 취소되었습니다.", "EasyZip", MessageBoxButton.OK, MessageBoxImage.Information);
                Close();
            }
            catch (Exception ex)
            {
                string diagnosis = ArchiveEngine.DiagnoseArchiveError(_archivePath ?? "", ex);
                MessageBox.Show($"압축 풀기 중 오류가 발생했습니다:\n\n{diagnosis}", "EasyZip 오류", MessageBoxButton.OK, MessageBoxImage.Error);
                Close();
            }
        }

        private async Task StartCompressionAsync()
        {
            var progress = new Progress<ExtractionProgressInfo>(UpdateUi);

            try
            {
                await ArchiveEngine.CompressAsync(_sourcePaths!, _destination, _compressionLevel, progress, _cts.Token);
                IsSuccess = true;

                if (ChkOpenFolder.IsChecked == true)
                {
                    string? dir = Path.GetDirectoryName(_destination);
                    if (!string.IsNullOrEmpty(dir) && Directory.Exists(dir))
                    {
                        Process.Start(new ProcessStartInfo
                        {
                            FileName = dir,
                            UseShellExecute = true
                        });
                    }
                }

                await Task.Delay(400);
                Close();
            }
            catch (OperationCanceledException)
            {
                MessageBox.Show("압축 작업이 취소되었습니다.", "EasyZip", MessageBoxButton.OK, MessageBoxImage.Information);
                Close();
            }
            catch (Exception ex)
            {
                MessageBox.Show($"압축 생성 중 오류가 발생했습니다:\n{ex.Message}", "EasyZip 오류", MessageBoxButton.OK, MessageBoxImage.Error);
                Close();
            }
        }

        private void UpdateUi(ExtractionProgressInfo info)
        {
            ProgressBarMain.Value = info.Percentage;
            TxtPercent.Text = $"{info.Percentage:F0}%";
            TxtCurrentFile.Text = string.IsNullOrEmpty(info.CurrentFile) ? info.StatusMessage : info.CurrentFile;
            TxtSpeed.Text = info.FormattedSpeed;
            TxtRemaining.Text = info.FormattedRemainingTime;
            TxtBytes.Text = $"{ArchiveItem.FormatBytes(info.ProcessedBytes)} / {ArchiveItem.FormatBytes(info.TotalBytes)}";
        }

        private void BtnCancel_Click(object sender, RoutedEventArgs e)
        {
            if (MessageBox.Show("진행 중인 작업을 취소하시겠습니까?", "확인", MessageBoxButton.YesNo, MessageBoxImage.Question) == MessageBoxResult.Yes)
            {
                _cts.Cancel();
                BtnCancel.IsEnabled = false;
                BtnCancel.Content = "취소 중...";
            }
        }

        protected override void OnClosing(System.ComponentModel.CancelEventArgs e)
        {
            SaveOpenFolderPreference();
            _cts.Cancel();
            base.OnClosing(e);
        }
    }
}
