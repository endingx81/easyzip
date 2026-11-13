using System;
using System.Diagnostics;
using System.IO;
using Microsoft.Win32;

namespace EasyZip.Services
{
    public class ShellIntegration
    {
        private static string GetExecutablePath()
        {
            return Process.GetCurrentProcess().MainModule?.FileName 
                   ?? Environment.ProcessPath 
                   ?? Path.Combine(AppDomain.CurrentDomain.BaseDirectory, "EasyZip.exe");
        }

        public const string ClsidString = "{D8F2A385-4C3D-4B9F-8349-3A65C82A4D10}";

        /// <summary>
        /// 윈도우 탐색기 마우스 우클릭 메뉴 전체 등록
        /// </summary>
        public static void RegisterContextMenu(
            bool extractHere = true,
            bool extractToFolder = true,
            bool openWith = true,
            bool compress = true,
            bool newFolder = true)
        {
            string exe = GetExecutablePath();
            string exeDir = Path.GetDirectoryName(exe) ?? "";
            string dllPath = Path.Combine(exeDir, "EasyZipShell.dll");

            // 64비트 C++ 쉘 익스텐션 DLL이 존재하면 동적 파일명 표시 컨텍스트 메뉴로 등록
            if (File.Exists(dllPath))
            {
                RegisterShellExtension(dllPath);

                // 동적 쉘 익스텐션이 메뉴를 담당하므로 정적 메뉴 중복 제거
                UnregisterStaticArchiveVerbs();
                UnregisterCompressContextMenu();
            }
            else
            {
                UnregisterShellExtension();

                // 1. EasyZip.Archive ProgID의 우클릭 메뉴 (압축파일용)
                RegisterArchiveContextMenu(exe, extractHere, extractToFolder, openWith);

                // 2. 확장자 직접 등록
                RegisterDirectExtensionContextMenu(exe, extractHere, extractToFolder, openWith);

                // 3. 파일 및 폴더 우클릭 시 압축하기 메뉴 (*, Directory)
                if (compress)
                {
                    RegisterCompressContextMenu(exe);
                }
                else
                {
                    UnregisterCompressContextMenu();
                }
            }

            // 기본 더블클릭 열기 등록은 ProgID에 항상 보장
            RegisterProgIdOpen(exe);

            // 4. 폴더 배경(빈 공간) 우클릭 시 "새 폴더 만들기" 및 탐색기 열기 (Directory\Background)
            if (newFolder)
            {
                RegisterBackgroundContextMenu(exe);
            }
            else
            {
                UnregisterBackgroundContextMenu();
            }

            NativeMethods.NotifyShellAssociationChanged();
        }

        public static void RegisterShellExtension(string dllPath)
        {
            if (!File.Exists(dllPath)) return;

            string clsidKey = $@"Software\Classes\CLSID\{ClsidString}";
            using (var k = Registry.CurrentUser.CreateSubKey(clsidKey))
            {
                k?.SetValue("", "EasyZip Shell Extension");
                using var inproc = k?.CreateSubKey("InprocServer32");
                inproc?.SetValue("", dllPath);
                inproc?.SetValue("ThreadingModel", "Apartment");
            }

            // ContextMenuHandlers 등록
            using (var k = Registry.CurrentUser.CreateSubKey(@"Software\Classes\*\shellex\ContextMenuHandlers\EasyZip"))
            {
                k?.SetValue("", ClsidString);
            }
            using (var k = Registry.CurrentUser.CreateSubKey(@"Software\Classes\Directory\shellex\ContextMenuHandlers\EasyZip"))
            {
                k?.SetValue("", ClsidString);
            }
            using (var k = Registry.CurrentUser.CreateSubKey(@"Software\Classes\Folder\shellex\ContextMenuHandlers\EasyZip"))
            {
                k?.SetValue("", ClsidString);
            }
        }

        public static void UnregisterShellExtension()
        {
            try
            {
                Registry.CurrentUser.DeleteSubKeyTree($@"Software\Classes\CLSID\{ClsidString}", false);
                Registry.CurrentUser.DeleteSubKeyTree(@"Software\Classes\*\shellex\ContextMenuHandlers\EasyZip", false);
                Registry.CurrentUser.DeleteSubKeyTree(@"Software\Classes\Directory\shellex\ContextMenuHandlers\EasyZip", false);
                Registry.CurrentUser.DeleteSubKeyTree(@"Software\Classes\Folder\shellex\ContextMenuHandlers\EasyZip", false);
            }
            catch { }
        }

        private static void RegisterProgIdOpen(string exe)
        {
            using var progIdKey = Registry.CurrentUser.CreateSubKey($@"Software\Classes\{FileAssociation.ProgId}\shell");
            if (progIdKey == null) return;
            using var openKey = progIdKey.CreateSubKey("open");
            openKey?.SetValue("", "EasyZip으로 열기(&O)");
            openKey?.SetValue("Icon", $"\"{exe}\",0");
            using var cmdKey = openKey?.CreateSubKey("command");
            cmdKey?.SetValue("", $"\"{exe}\" \"%1\"");
        }

        private static void UnregisterStaticArchiveVerbs()
        {
            try
            {
                Registry.CurrentUser.DeleteSubKeyTree($@"Software\Classes\{FileAssociation.ProgId}\shell\EasyZip.ExtractHere", false);
                Registry.CurrentUser.DeleteSubKeyTree($@"Software\Classes\{FileAssociation.ProgId}\shell\EasyZip.ExtractToFolder", false);
                Registry.CurrentUser.DeleteSubKeyTree($@"Software\Classes\{FileAssociation.ProgId}\shell\EasyZip.Open", false);

                foreach (var ext in FileAssociation.AllSupportedExtensions)
                {
                    Registry.CurrentUser.DeleteSubKeyTree($@"Software\Classes\SystemFileAssociations\{ext}\shell\EasyZip.ExtractHere", false);
                    Registry.CurrentUser.DeleteSubKeyTree($@"Software\Classes\SystemFileAssociations\{ext}\shell\EasyZip.ExtractToFolder", false);
                    Registry.CurrentUser.DeleteSubKeyTree($@"Software\Classes\SystemFileAssociations\{ext}\shell\EasyZip.Open", false);
                }
            }
            catch { }
        }

        private static void RegisterArchiveContextMenu(string exe, bool extractHere, bool extractToFolder, bool openWith)
        {
            using var progIdKey = Registry.CurrentUser.CreateSubKey($@"Software\Classes\{FileAssociation.ProgId}\shell");
            if (progIdKey == null) return;

            // 중복된 EasyZip.Open 키가 남아있으면 제거하여 "열기" 중복 방지
            try { Registry.CurrentUser.DeleteSubKeyTree($@"Software\Classes\{FileAssociation.ProgId}\shell\EasyZip.Open", false); } catch { }

            // 1. 기본 열기 동사 (open)
            if (openWith)
            {
                using var openKey = progIdKey.CreateSubKey("open");
                openKey?.SetValue("", "EasyZip으로 열기(&O)");
                openKey?.SetValue("Icon", $"\"{exe}\",0");
                using var cmdKey = openKey?.CreateSubKey("command");
                cmdKey?.SetValue("", $"\"{exe}\" \"%1\"");
            }

            // 2. 여기에 압축 풀기
            if (extractHere)
            {
                using var exHereKey = progIdKey.CreateSubKey("EasyZip.ExtractHere");
                exHereKey?.SetValue("", "EasyZip으로 여기에 압축 풀기(&X)");
                exHereKey?.SetValue("Icon", $"\"{exe}\",0");
                using var cmdKey = exHereKey?.CreateSubKey("command");
                cmdKey?.SetValue("", $"\"{exe}\" -x \"%1\"");
            }

            // 3. 각각의 폴더에 압축 풀기
            if (extractToFolder)
            {
                using var exFolderKey = progIdKey.CreateSubKey("EasyZip.ExtractToFolder");
                exFolderKey?.SetValue("", "EasyZip으로 각각의 폴더에 압축 풀기(&E)");
                exFolderKey?.SetValue("Icon", $"\"{exe}\",0");
                using var cmdKey = exFolderKey?.CreateSubKey("command");
                cmdKey?.SetValue("", $"\"{exe}\" -xd \"%1\"");
            }
        }

        private static void RegisterDirectExtensionContextMenu(string exe, bool extractHere, bool extractToFolder, bool openWith)
        {
            foreach (var ext in FileAssociation.AllSupportedExtensions)
            {
                try
                {
                    // 과거에 등록되었던 SystemFileAssociations 내의 EasyZip.Open 제거 (열기 2개 중복 노출 원인 제거)
                    try { Registry.CurrentUser.DeleteSubKeyTree($@"Software\Classes\SystemFileAssociations\{ext}\shell\EasyZip.Open", false); } catch { }

                    using var shellKey = Registry.CurrentUser.CreateSubKey($@"Software\Classes\SystemFileAssociations\{ext}\shell");
                    if (shellKey == null) continue;

                    if (extractHere)
                    {
                        using var k = shellKey.CreateSubKey("EasyZip.ExtractHere");
                        k?.SetValue("", "EasyZip으로 여기에 압축 풀기(&X)");
                        k?.SetValue("Icon", $"\"{exe}\",0");
                        using var cmd = k?.CreateSubKey("command");
                        cmd?.SetValue("", $"\"{exe}\" -x \"%1\"");
                    }

                    if (extractToFolder)
                    {
                        using var k = shellKey.CreateSubKey("EasyZip.ExtractToFolder");
                        k?.SetValue("", "EasyZip으로 각각의 폴더에 압축 풀기(&E)");
                        k?.SetValue("Icon", $"\"{exe}\",0");
                        using var cmd = k?.CreateSubKey("command");
                        cmd?.SetValue("", $"\"{exe}\" -xd \"%1\"");
                    }
                }
                catch
                {
                    // Ignore
                }
            }
        }

        private static void RegisterCompressContextMenu(string exe)
        {
            string[] targets = { @"Software\Classes\*", @"Software\Classes\Directory" };
            foreach (var target in targets)
            {
                try
                {
                    // 1. EasyZip으로 압축하기 (대화상자)
                    using var compKey = Registry.CurrentUser.CreateSubKey($@"{target}\shell\EasyZip.Compress");
                    if (compKey != null)
                    {
                        compKey.SetValue("", "EasyZip으로 압축하기(&C)...");
                        compKey.SetValue("Icon", $"\"{exe}\",0");
                        using var cmd = compKey.CreateSubKey("command");
                        cmd?.SetValue("", $"\"{exe}\" -c \"%1\"");
                    }

                    // 2. 빠른 ZIP 압축
                    using var quickZipKey = Registry.CurrentUser.CreateSubKey($@"{target}\shell\EasyZip.QuickZip");
                    if (quickZipKey != null)
                    {
                        quickZipKey.SetValue("", "EasyZip으로 빠른 ZIP 압축(&Z)");
                        quickZipKey.SetValue("Icon", $"\"{exe}\",0");
                        using var cmd = quickZipKey.CreateSubKey("command");
                        cmd?.SetValue("", $"\"{exe}\" -cz \"%1\"");
                    }
                }
                catch
                {
                    // Ignore
                }
            }
        }

        private static void UnregisterCompressContextMenu()
        {
            string[] targets = { @"Software\Classes\*", @"Software\Classes\Directory" };
            foreach (var target in targets)
            {
                try
                {
                    Registry.CurrentUser.DeleteSubKeyTree($@"{target}\shell\EasyZip.Compress", false);
                    Registry.CurrentUser.DeleteSubKeyTree($@"{target}\shell\EasyZip.QuickZip", false);
                }
                catch
                {
                    // Ignore
                }
            }
        }

        private static void RegisterBackgroundContextMenu(string exe)
        {
            try
            {
                // 윈도우 탐색기 폴더 빈 공간 및 바탕화면 우클릭 메뉴 최상단에 "새 폴더 만들기" 등록
                string[] bgTargets = {
                    @"Software\Classes\Directory\Background\shell\EasyZip.NewFolder",
                    @"Software\Classes\DesktopBackground\shell\EasyZip.NewFolder"
                };

                foreach (var target in bgTargets)
                {
                    using var newFolderKey = Registry.CurrentUser.CreateSubKey(target);
                    if (newFolderKey != null)
                    {
                        newFolderKey.SetValue("", "새 폴더 만들기(&N)");
                        newFolderKey.SetValue("Position", "Top");
                        newFolderKey.SetValue("Icon", "shell32.dll,3");
                        using var cmd = newFolderKey.CreateSubKey("command");
                        cmd?.SetValue("", $"\"{exe}\" -newfolder \"%V\"");
                    }
                }

                // 기존 자체 탐색기 열기 메뉴는 깔끔하게 삭제
                Registry.CurrentUser.DeleteSubKeyTree(@"Software\Classes\Directory\Background\shell\EasyZip.OpenExplorer", false);
                Registry.CurrentUser.DeleteSubKeyTree(@"Software\Classes\DesktopBackground\shell\EasyZip.OpenExplorer", false);
            }
            catch
            {
                // Ignore
            }
        }

        private static void UnregisterBackgroundContextMenu()
        {
            try
            {
                Registry.CurrentUser.DeleteSubKeyTree(@"Software\Classes\Directory\Background\shell\EasyZip.NewFolder", false);
                Registry.CurrentUser.DeleteSubKeyTree(@"Software\Classes\DesktopBackground\shell\EasyZip.NewFolder", false);
                Registry.CurrentUser.DeleteSubKeyTree(@"Software\Classes\Directory\Background\shell\EasyZip.OpenExplorer", false);
                Registry.CurrentUser.DeleteSubKeyTree(@"Software\Classes\DesktopBackground\shell\EasyZip.OpenExplorer", false);
            }
            catch
            {
                // Ignore
            }
        }

        /// <summary>
        /// 윈도우 탐색기 마우스 우클릭 메뉴 전체 해제
        /// </summary>
        public static void UnregisterContextMenu()
        {
            try
            {
                UnregisterShellExtension();

                Registry.CurrentUser.DeleteSubKeyTree($@"Software\Classes\{FileAssociation.ProgId}\shell\EasyZip.ExtractHere", false);
                Registry.CurrentUser.DeleteSubKeyTree($@"Software\Classes\{FileAssociation.ProgId}\shell\EasyZip.ExtractToFolder", false);
                Registry.CurrentUser.DeleteSubKeyTree($@"Software\Classes\{FileAssociation.ProgId}\shell\Open", false);

                foreach (var ext in FileAssociation.AllSupportedExtensions)
                {
                    Registry.CurrentUser.DeleteSubKeyTree($@"Software\Classes\SystemFileAssociations\{ext}\shell\EasyZip.ExtractHere", false);
                    Registry.CurrentUser.DeleteSubKeyTree($@"Software\Classes\SystemFileAssociations\{ext}\shell\EasyZip.ExtractToFolder", false);
                    Registry.CurrentUser.DeleteSubKeyTree($@"Software\Classes\SystemFileAssociations\{ext}\shell\EasyZip.Open", false);
                }

                UnregisterCompressContextMenu();
                UnregisterBackgroundContextMenu();
            }
            catch
            {
                // Ignore
            }

            NativeMethods.NotifyShellAssociationChanged();
        }

        public static bool IsContextMenuRegistered()
        {
            try
            {
                using var k = Registry.CurrentUser.OpenSubKey(@"Software\Classes\*\shellex\ContextMenuHandlers\EasyZip");
                if (k != null) return true;
                using var k2 = Registry.CurrentUser.OpenSubKey(@"Software\Classes\Directory\Background\shell\EasyZip.NewFolder");
                return k2 != null;
            }
            catch
            {
                return false;
            }
        }
    }
}

