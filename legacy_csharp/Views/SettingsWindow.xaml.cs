using System;
using System.Collections.Generic;
using System.Linq;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Media;
using EasyZip.Models;
using EasyZip.Services;

namespace EasyZip.Views
{
    public partial class SettingsWindow : Window
    {
        private readonly List<CheckBox> _extCheckBoxes = new();
        private readonly AppSettings _settings;

        public SettingsWindow()
        {
            InitializeComponent();
            _settings = AppSettings.Load();

            LoadSettingsToUi();
            PopulateExtensions();
        }

        private void LoadSettingsToUi()
        {
            ChkExtractHere.IsChecked = _settings.ContextMenuExtractHere;
            ChkExtractToFolder.IsChecked = _settings.ContextMenuExtractToFolder;
            ChkOpenWith.IsChecked = _settings.ContextMenuOpenWithEasyZip;
            ChkCompress.IsChecked = _settings.ContextMenuCompress;
            ChkQuickZip.IsChecked = _settings.ContextMenuCompress;
            ChkNewFolder.IsChecked = _settings.ContextMenuNewFolder;

            ChkMultiThread.IsChecked = _settings.MultiThreadExtraction;
            ChkOpenDestFolder.IsChecked = _settings.OpenFolderAfterExtraction;

            TxtCpuInfo.Text = $"CPU 논리 프로세서: {Environment.ProcessorCount} 코어 / 스레드";

            if (ShellIntegration.IsContextMenuRegistered())
            {
                TxtContextMenuStatus.Text = "✓ 현재 윈도우 탐색기 우클릭 메뉴가 등록되어 있습니다.";
                TxtContextMenuStatus.Foreground = new SolidColorBrush(Color.FromRgb(22, 163, 74));
            }
            else
            {
                TxtContextMenuStatus.Text = "ℹ 아직 윈도우 탐색기 우클릭 메뉴가 등록되지 않았습니다.";
                TxtContextMenuStatus.Foreground = new SolidColorBrush(Color.FromRgb(202, 138, 4));
            }
        }

        private void PopulateExtensions()
        {
            WrapExtensions.Children.Clear();
            _extCheckBoxes.Clear();

            foreach (var ext in FileAssociation.AllSupportedExtensions)
            {
                bool isAssoc = FileAssociation.IsAssociated(ext, out string currentHandler);

                var panel = new StackPanel { Orientation = Orientation.Horizontal };
                var cb = new CheckBox
                {
                    Content = ext,
                    Tag = ext,
                    IsChecked = isAssoc || _settings.AssociatedExtensions.Contains(ext),
                    VerticalAlignment = VerticalAlignment.Center,
                    Margin = new Thickness(0, 0, 4, 0),
                    FontWeight = FontWeights.Medium
                };

                string badgeText;
                Brush badgeColor;

                if (isAssoc)
                {
                    badgeText = "(✓연결됨)";
                    badgeColor = new SolidColorBrush(Color.FromRgb(22, 163, 74)); // 녹색
                }
                else if (!string.IsNullOrEmpty(currentHandler))
                {
                    badgeText = $"({currentHandler})";
                    badgeColor = new SolidColorBrush(Color.FromRgb(217, 119, 6)); // 주황색 (다른 프로그램이 선점)
                }
                else
                {
                    badgeText = "(미연결)";
                    badgeColor = new SolidColorBrush(Color.FromRgb(156, 163, 175)); // 회색
                }

                var statusText = new TextBlock
                {
                    Text = badgeText,
                    FontSize = 10,
                    Foreground = badgeColor,
                    VerticalAlignment = VerticalAlignment.Center
                };

                panel.Children.Add(cb);
                panel.Children.Add(statusText);

                _extCheckBoxes.Add(cb);
                WrapExtensions.Children.Add(panel);
            }
        }

        private void BtnApplyContextMenu_Click(object sender, RoutedEventArgs e)
        {
            try
            {
                bool extractHere = ChkExtractHere.IsChecked == true;
                bool extractToFolder = ChkExtractToFolder.IsChecked == true;
                bool openWith = ChkOpenWith.IsChecked == true;
                bool compress = ChkCompress.IsChecked == true || ChkQuickZip.IsChecked == true;
                bool newFolder = ChkNewFolder.IsChecked == true;

                ShellIntegration.RegisterContextMenu(extractHere, extractToFolder, openWith, compress, newFolder);

                _settings.ContextMenuExtractHere = extractHere;
                _settings.ContextMenuExtractToFolder = extractToFolder;
                _settings.ContextMenuOpenWithEasyZip = openWith;
                _settings.ContextMenuCompress = compress;
                _settings.ContextMenuNewFolder = newFolder;
                _settings.Save();

                TxtContextMenuStatus.Text = "✓ 윈도우 탐색기 우클릭 메뉴가 성공적으로 등록되었습니다!";
                TxtContextMenuStatus.Foreground = new SolidColorBrush(Color.FromRgb(22, 163, 74));
                MessageBox.Show("윈도우 탐색기 마우스 우클릭 메뉴가 성공적으로 적용되었습니다.\n(탐색기에서 파일, 폴더, 빈 화면 우클릭 시 즉시 확인 가능)", 
                                "EasyZip", MessageBoxButton.OK, MessageBoxImage.Information);
            }
            catch (Exception ex)
            {
                MessageBox.Show($"우클릭 메뉴 등록 중 오류가 발생했습니다:\n{ex.Message}", "오류", MessageBoxButton.OK, MessageBoxImage.Error);
            }
        }

        private void BtnRemoveContextMenu_Click(object sender, RoutedEventArgs e)
        {
            try
            {
                ShellIntegration.UnregisterContextMenu();
                TxtContextMenuStatus.Text = "우클릭 메뉴가 모두 해제되었습니다.";
                TxtContextMenuStatus.Foreground = new SolidColorBrush(Color.FromRgb(107, 114, 128));
                MessageBox.Show("윈도우 탐색기 마우스 우클릭 메뉴가 해제되었습니다.", "EasyZip", MessageBoxButton.OK, MessageBoxImage.Information);
            }
            catch (Exception ex)
            {
                MessageBox.Show($"우클릭 메뉴 해제 중 오류가 발생했습니다:\n{ex.Message}", "오류", MessageBoxButton.OK, MessageBoxImage.Error);
            }
        }

        private void BtnSelectAllExt_Click(object sender, RoutedEventArgs e)
        {
            foreach (var cb in _extCheckBoxes)
            {
                cb.IsChecked = true;
            }
        }

        private void BtnDeselectAllExt_Click(object sender, RoutedEventArgs e)
        {
            foreach (var cb in _extCheckBoxes)
            {
                cb.IsChecked = false;
            }
        }

        private void BtnApplyAssociations_Click(object sender, RoutedEventArgs e)
        {
            try
            {
                var selected = _extCheckBoxes
                    .Where(cb => cb.IsChecked == true)
                    .Select(cb => cb.Tag.ToString()!)
                    .ToList();

                FileAssociation.AssociateAll(selected);

                // 해제된 것들 처리
                var unselected = _extCheckBoxes
                    .Where(cb => cb.IsChecked != true)
                    .Select(cb => cb.Tag.ToString()!);
                FileAssociation.UnassociateAll(unselected);

                _settings.AssociatedExtensions = selected;
                _settings.Save();

                PopulateExtensions();

                // 연결 적용 후 여전히 다른 앱(UserChoice)이 선점하고 있는지 확인
                var blockedList = new List<string>();
                foreach (var ext in selected)
                {
                    if (!FileAssociation.IsAssociated(ext, out string handler) && !string.IsNullOrEmpty(handler))
                    {
                        blockedList.Add($"{ext} (현재: {handler})");
                    }
                }

                if (blockedList.Count > 0)
                {
                    TxtAssocStatus.Text = $"⚠️ Windows 보안 정책(UserChoice)에 의해 {blockedList.Count}개 확장자가 기존 프로그램에 고정되어 있습니다.\n[Windows 기본 앱 설정 열기]를 눌러 EasyZip을 기본 앱으로 선택해 주세요.";
                    TxtAssocStatus.Foreground = new SolidColorBrush(Color.FromRgb(217, 119, 6));

                    if (MessageBox.Show(
                        $"일부 확장자({string.Join(", ", blockedList.Take(3))})는 Windows 보안 정책에 의해 이전 프로그램이 고정되어 있습니다.\n\n[Windows 기본 앱 설정] 화면을 열어 EasyZip을 기본 앱으로 지정하시겠습니까?",
                        "Windows 기본 앱 설정 안내",
                        MessageBoxButton.YesNo,
                        MessageBoxImage.Information) == MessageBoxResult.Yes)
                    {
                        FileAssociation.OpenWindowsDefaultAppsSettings();
                    }
                }
                else
                {
                    TxtAssocStatus.Text = $"✓ {selected.Count}개 확장자가 EasyZip으로 성공적으로 등록되었습니다!";
                    TxtAssocStatus.Foreground = new SolidColorBrush(Color.FromRgb(22, 163, 74));
                    MessageBox.Show($"선택한 {selected.Count}개 확장자가 EasyZip에 성공적으로 등록되었습니다.", 
                                    "EasyZip", MessageBoxButton.OK, MessageBoxImage.Information);
                }
            }
            catch (Exception ex)
            {
                MessageBox.Show($"확장자 연결 중 오류가 발생했습니다:\n{ex.Message}", "오류", MessageBoxButton.OK, MessageBoxImage.Error);
            }
        }

        private void BtnOpenDefaultApps_Click(object sender, RoutedEventArgs e)
        {
            FileAssociation.OpenWindowsDefaultAppsSettings();
        }

        private void BtnUnassociateAll_Click(object sender, RoutedEventArgs e)
        {
            try
            {
                FileAssociation.UnassociateAll();
                _settings.AssociatedExtensions.Clear();
                _settings.Save();

                PopulateExtensions();
                TxtAssocStatus.Text = "확장자 연결이 모두 초기화되었습니다.";
                MessageBox.Show("모든 파일 확장자 연결이 해제되었습니다.", "EasyZip", MessageBoxButton.OK, MessageBoxImage.Information);
            }
            catch (Exception ex)
            {
                MessageBox.Show($"확장자 연결 해제 중 오류:\n{ex.Message}", "오류", MessageBoxButton.OK, MessageBoxImage.Error);
            }
        }

        private void BtnClose_Click(object sender, RoutedEventArgs e)
        {
            _settings.MultiThreadExtraction = ChkMultiThread.IsChecked == true;
            _settings.OpenFolderAfterExtraction = ChkOpenDestFolder.IsChecked == true;
            _settings.Save();

            Close();
        }
    }
}

