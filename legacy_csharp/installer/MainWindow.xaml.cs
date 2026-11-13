using System;
using System.Diagnostics;
using System.IO;
using System.IO.Compression;
using System.Reflection;
using System.Threading.Tasks;
using System.Windows;
using Microsoft.Win32;

namespace EasyZipSetup
{
    public partial class MainWindow : Window
    {
        private readonly string _defaultInstallDir;

        public MainWindow()
        {
            InitializeComponent();

            _defaultInstallDir = Path.Combine(
                Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData),
                "Programs",
                "EasyZip"
            );

            TxtInstallPath.Text = _defaultInstallDir;
        }

        private void BtnBrowse_Click(object sender, RoutedEventArgs e)
        {
            var dialog = new OpenFolderDialog
            {
                Title = "EasyZip 설치 폴더 선택",
                InitialDirectory = TxtInstallPath.Text
            };

            if (dialog.ShowDialog() == true)
            {
                TxtInstallPath.Text = Path.Combine(dialog.FolderName, "EasyZip");
            }
        }

        private void BtnCancel_Click(object sender, RoutedEventArgs e)
        {
            Close();
        }

        private async void BtnInstall_Click(object sender, RoutedEventArgs e)
        {
            string installDir = TxtInstallPath.Text.Trim();
            if (string.IsNullOrEmpty(installDir))
            {
                MessageBox.Show("설치 폴더를 지정해 주세요.", "알림", MessageBoxButton.OK, MessageBoxImage.Warning);
                return;
            }

            PanelOptions.Visibility = Visibility.Collapsed;
            PanelProgress.Visibility = Visibility.Visible;
            BtnInstall.IsEnabled = false;
            BtnCancel.IsEnabled = false;

            bool createDesktop = ChkDesktopShortcut.IsChecked == true;
            bool createStart = ChkStartMenu.IsChecked == true;
            bool registerShell = ChkContextMenu.IsChecked == true;
            bool registerAssoc = ChkFileAssoc.IsChecked == true;
            bool launchAfter = ChkLaunchAfter.IsChecked == true;

            try
            {
                await Task.Run(() => PerformInstallation(installDir, createDesktop, createStart, registerShell, registerAssoc));

                TxtProgressStatus.Text = "설치가 완료되었습니다!";
                TxtDetailStatus.Text = "EasyZip이 PC에 성공적으로 설치되었습니다.";

                if (launchAfter)
                {
                    string exePath = Path.Combine(installDir, "EasyZip.exe");
                    if (File.Exists(exePath))
                    {
                        Process.Start(new ProcessStartInfo { FileName = exePath, UseShellExecute = true });
                    }
                }

                MessageBox.Show("EasyZip 설치가 성공적으로 완료되었습니다!", "EasyZip 설치 완료", MessageBoxButton.OK, MessageBoxImage.Information);
                Close();
            }
            catch (Exception ex)
            {
                MessageBox.Show($"설치 중 오류가 발생했습니다:\n{ex.Message}", "설치 오류", MessageBoxButton.OK, MessageBoxImage.Error);
                PanelOptions.Visibility = Visibility.Visible;
                PanelProgress.Visibility = Visibility.Collapsed;
                BtnInstall.IsEnabled = true;
                BtnCancel.IsEnabled = true;
            }
        }

        private void PerformInstallation(string installDir, bool createDesktop, bool createStart, bool registerShell, bool registerAssoc)
        {
            // 1. 디렉터리 생성
            if (!Directory.Exists(installDir))
            {
                Directory.CreateDirectory(installDir);
            }

            // 2. Embedded Resource에서 payload.zip 추출
            var asm = Assembly.GetExecutingAssembly();
            using var resourceStream = asm.GetManifestResourceStream("EasyZipSetup.payload.zip");
            if (resourceStream == null)
            {
                throw new InvalidOperationException("설치 리소스(payload.zip)를 찾을 수 없습니다.");
            }

            using (var zipArchive = new ZipArchive(resourceStream, ZipArchiveMode.Read))
            {
                foreach (var entry in zipArchive.Entries)
                {
                    string destPath = Path.Combine(installDir, entry.FullName);
                    if (entry.FullName.EndsWith('/') || entry.FullName.EndsWith('\\'))
                    {
                        if (!Directory.Exists(destPath)) Directory.CreateDirectory(destPath);
                    }
                    else
                    {
                        string? parent = Path.GetDirectoryName(destPath);
                        if (!string.IsNullOrEmpty(parent) && !Directory.Exists(parent))
                        {
                            Directory.CreateDirectory(parent);
                        }
                        entry.ExtractToFile(destPath, true);
                    }
                }
            }

            string exePath = Path.Combine(installDir, "EasyZip.exe");

            // 3. 바로가기 생성
            if (createDesktop)
            {
                string desktopFolder = Environment.GetFolderPath(Environment.SpecialFolder.DesktopDirectory);
                string shortcutPath = Path.Combine(desktopFolder, "EasyZip.lnk");
                CreateShortcut(shortcutPath, exePath, "EasyZip 초고속 압축 & 탐색기");
            }

            if (createStart)
            {
                string startPrograms = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.Programs), "EasyZip");
                if (!Directory.Exists(startPrograms)) Directory.CreateDirectory(startPrograms);
                string shortcutPath = Path.Combine(startPrograms, "EasyZip.lnk");
                CreateShortcut(shortcutPath, exePath, "EasyZip 초고속 압축 & 탐색기");
            }

            // 4. 레지스트리 쉘 메뉴 등록
            if (registerShell)
            {
                RegisterContextMenu(exePath);
            }

            // 5. 파일 확장자 연결 등록 (.alz, .egg 포함)
            if (registerAssoc)
            {
                RegisterFileAssociations(exePath);
            }

            // 6. 제어판 프로그램 추가/제거 (앱 및 기능) 등록
            RegisterUninstallEntry(installDir, exePath);

            // 7. Shell 변경 사항 브로드캐스트
            NotifyShell();
        }

        private static void CreateShortcut(string shortcutPath, string targetPath, string description)
        {
            try
            {
                Type? shellType = Type.GetTypeFromProgID("WScript.Shell");
                if (shellType == null) return;

                dynamic shell = Activator.CreateInstance(shellType)!;
                dynamic shortcut = shell.CreateShortcut(shortcutPath);
                shortcut.TargetPath = targetPath;
                shortcut.WorkingDirectory = Path.GetDirectoryName(targetPath);
                shortcut.Description = description;
                shortcut.IconLocation = targetPath + ",0";
                shortcut.Save();
            }
            catch { }
        }

        private static void RegisterContextMenu(string exe)
        {
            try
            {
                string exeDir = Path.GetDirectoryName(exe) ?? "";
                string dllPath = Path.Combine(exeDir, "EasyZipShell.dll");
                const string clsidString = "{D8F2A385-4C3D-4B9F-8349-3A65C82A4D10}";

                if (File.Exists(dllPath))
                {
                    // 64비트 C++ 쉘 익스텐션 등록 (동적 파일명 표시 지원)
                    string clsidKey = $@"Software\Classes\CLSID\{clsidString}";
                    using (var k = Registry.CurrentUser.CreateSubKey(clsidKey))
                    {
                        k?.SetValue("", "EasyZip Shell Extension");
                        using var inproc = k?.CreateSubKey("InprocServer32");
                        inproc?.SetValue("", dllPath);
                        inproc?.SetValue("ThreadingModel", "Apartment");
                    }

                    using (var k = Registry.CurrentUser.CreateSubKey(@"Software\Classes\*\shellex\ContextMenuHandlers\EasyZip"))
                    {
                        k?.SetValue("", clsidString);
                    }
                    using (var k = Registry.CurrentUser.CreateSubKey(@"Software\Classes\Directory\shellex\ContextMenuHandlers\EasyZip"))
                    {
                        k?.SetValue("", clsidString);
                    }
                    using (var k = Registry.CurrentUser.CreateSubKey(@"Software\Classes\Folder\shellex\ContextMenuHandlers\EasyZip"))
                    {
                        k?.SetValue("", clsidString);
                    }

                    // 정적 메뉴 중복 제거
                    const string constProgId = "EasyZip.Archive";
                    Registry.CurrentUser.DeleteSubKeyTree($@"Software\Classes\{constProgId}\shell\EasyZip.ExtractHere", false);
                    Registry.CurrentUser.DeleteSubKeyTree($@"Software\Classes\{constProgId}\shell\EasyZip.ExtractToFolder", false);
                    Registry.CurrentUser.DeleteSubKeyTree($@"Software\Classes\{constProgId}\shell\EasyZip.Open", false);

                    string[] archiveExts = {
                        ".zip", ".7z", ".rar", ".tar", ".gz", ".tgz",
                        ".bz2", ".tbz2", ".xz", ".txz", ".iso", ".cab",
                        ".zst", ".tzst", ".lz", ".lzh", ".alz", ".egg",
                        ".001"
                    };
                    foreach (var ext in archiveExts)
                    {
                        Registry.CurrentUser.DeleteSubKeyTree($@"Software\Classes\SystemFileAssociations\{ext}\shell\EasyZip.ExtractHere", false);
                        Registry.CurrentUser.DeleteSubKeyTree($@"Software\Classes\SystemFileAssociations\{ext}\shell\EasyZip.ExtractToFolder", false);
                        Registry.CurrentUser.DeleteSubKeyTree($@"Software\Classes\SystemFileAssociations\{ext}\shell\EasyZip.Open", false);
                    }

                    Registry.CurrentUser.DeleteSubKeyTree(@"Software\Classes\*\shell\EasyZip.Compress", false);
                    Registry.CurrentUser.DeleteSubKeyTree(@"Software\Classes\*\shell\EasyZip.QuickZip", false);
                    Registry.CurrentUser.DeleteSubKeyTree(@"Software\Classes\Directory\shell\EasyZip.Compress", false);
                    Registry.CurrentUser.DeleteSubKeyTree(@"Software\Classes\Directory\shell\EasyZip.QuickZip", false);
                }

                // 더블클릭 기본 열기 보장
                const string progId = "EasyZip.Archive";
                using var progKey = Registry.CurrentUser.CreateSubKey($@"Software\Classes\{progId}\shell");
                if (progKey != null)
                {
                    using (var openKey = progKey.CreateSubKey("Open"))
                    {
                        openKey?.SetValue("", "EasyZip으로 열기(&O)");
                        openKey?.SetValue("Icon", $"\"{exe}\",0");
                        using var cmd = openKey?.CreateSubKey("command");
                        cmd?.SetValue("", $"\"{exe}\" \"%1\"");
                    }
                }

                // 디렉터리 빈 공간 (바탕화면 / 탐색기 배경) 우클릭 최상단: 새 폴더 만들기!
                string[] bgTargets = {
                    @"Software\Classes\Directory\Background\shell\EasyZip.NewFolder",
                    @"Software\Classes\DesktopBackground\shell\EasyZip.NewFolder"
                };

                foreach (var target in bgTargets)
                {
                    using var bgNewFolder = Registry.CurrentUser.CreateSubKey(target);
                    if (bgNewFolder != null)
                    {
                        bgNewFolder.SetValue("", "새 폴더 만들기(&N)");
                        bgNewFolder.SetValue("Position", "Top");
                        bgNewFolder.SetValue("Icon", "shell32.dll,3");
                        using var cmd = bgNewFolder.CreateSubKey("command");
                        cmd?.SetValue("", $"\"{exe}\" -newfolder \"%V\"");
                    }
                }

                Registry.CurrentUser.DeleteSubKeyTree(@"Software\Classes\Directory\Background\shell\EasyZip.OpenExplorer", false);
                Registry.CurrentUser.DeleteSubKeyTree(@"Software\Classes\DesktopBackground\shell\EasyZip.OpenExplorer", false);
            }
            catch { }
        }

        private static void RegisterFileAssociations(string exe)
        {
            string[] extensions =
            {
                ".zip", ".7z", ".rar", ".tar", ".gz", ".tgz",
                ".bz2", ".tbz2", ".xz", ".txz", ".iso", ".cab",
                ".zst", ".tzst", ".lz", ".lzh", ".alz", ".egg",
                ".001"
            };

            const string progId = "EasyZip.Archive";
            using (var progIdKey = Registry.CurrentUser.CreateSubKey($@"Software\Classes\{progId}"))
            {
                progIdKey?.SetValue("", "EasyZip 압축 파일");
                using var iconKey = progIdKey?.CreateSubKey("DefaultIcon");
                iconKey?.SetValue("", $"\"{exe}\",0");
                using var cmdKey = progIdKey?.CreateSubKey(@"shell\open\command");
                cmdKey?.SetValue("", $"\"{exe}\" \"%1\"");
            }

            // Applications 등록 (Windows '연결 프로그램' 목록용)
            try
            {
                using var appCmd = Registry.CurrentUser.CreateSubKey(@"Software\Classes\Applications\EasyZip.exe\shell\open\command");
                appCmd?.SetValue("", $"\"{exe}\" \"%1\"");
            }
            catch { }

            // 1. RegisteredApplications & Capabilities (HKCU & HKLM)
            try
            {
                using var appRegKey = Registry.CurrentUser.CreateSubKey(@"Software\RegisteredApplications");
                appRegKey?.SetValue("EasyZip", @"Software\EasyZip\Capabilities");

                using var capKey = Registry.CurrentUser.CreateSubKey(@"Software\EasyZip\Capabilities");
                if (capKey != null)
                {
                    capKey.SetValue("ApplicationName", "EasyZip");
                    capKey.SetValue("ApplicationDescription", "초고속 멀티스레드 압축 & 알집 지원 자체 탐색기");

                    using var fileAssocKey = capKey.CreateSubKey("FileAssociations");
                    if (fileAssocKey != null)
                    {
                        foreach (var ext in extensions)
                        {
                            fileAssocKey.SetValue(ext, progId);
                        }
                    }
                }
            }
            catch { }

            try
            {
                using var hklmAppReg = Registry.LocalMachine.CreateSubKey(@"Software\RegisteredApplications");
                hklmAppReg?.SetValue("EasyZip", @"Software\EasyZip\Capabilities");

                using var hklmCap = Registry.LocalMachine.CreateSubKey(@"Software\EasyZip\Capabilities");
                if (hklmCap != null)
                {
                    hklmCap.SetValue("ApplicationName", "EasyZip");
                    hklmCap.SetValue("ApplicationDescription", "초고속 멀티스레드 압축 & 알집 지원 자체 탐색기");

                    using var fileAssocKey = hklmCap.CreateSubKey("FileAssociations");
                    if (fileAssocKey != null)
                    {
                        foreach (var ext in extensions)
                        {
                            fileAssocKey.SetValue(ext, progId);
                        }
                    }
                }
            }
            catch { }

            foreach (var ext in extensions)
            {
                try
                {
                    using var extKey = Registry.CurrentUser.CreateSubKey($@"Software\Classes\{ext}");
                    extKey?.SetValue("", progId);

                    using var openWith = extKey?.CreateSubKey("OpenWithProgids");
                    openWith?.SetValue(progId, "");

                    using var openWithList = extKey?.CreateSubKey(@"OpenWithList\EasyZip.exe");

                    using var fileExt = Registry.CurrentUser.CreateSubKey($@"Software\Microsoft\Windows\CurrentVersion\Explorer\FileExts\{ext}\OpenWithProgids");
                    fileExt?.SetValue(progId, new byte[0], RegistryValueKind.None);
                }
                catch { }
            }
        }

        private static void RegisterUninstallEntry(string installDir, string exePath)
        {
            try
            {
                const string uninstallKey = @"Software\Microsoft\Windows\CurrentVersion\Uninstall\EasyZip";
                using var key = Registry.CurrentUser.CreateSubKey(uninstallKey);
                if (key != null)
                {
                    key.SetValue("DisplayName", "EasyZip (초고속 압축 & 알집 지원)");
                    key.SetValue("DisplayVersion", "1.0.0");
                    key.SetValue("Publisher", "EasyZip Team");
                    key.SetValue("InstallLocation", installDir);
                    key.SetValue("DisplayIcon", $"\"{exePath}\",0");
                    key.SetValue("UninstallString", $"\"{exePath}\" --uninstall");
                    key.SetValue("NoModify", 1, RegistryValueKind.DWord);
                    key.SetValue("NoRepair", 1, RegistryValueKind.DWord);
                }
            }
            catch { }
        }

        private static void NotifyShell()
        {
            try
            {
                NativeShell.SHChangeNotify(0x08000000, 0x0000, IntPtr.Zero, IntPtr.Zero);
            }
            catch { }
        }
    }

    internal static class NativeShell
    {
        [System.Runtime.InteropServices.DllImport("shell32.dll")]
        public static extern void SHChangeNotify(uint wEventId, uint uFlags, IntPtr dwItem1, IntPtr dwItem2);
    }
}

