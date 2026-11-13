using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Runtime.InteropServices;
using System.Security.AccessControl;
using Microsoft.Win32;

namespace EasyZip.Services
{
    public class FileAssociation
    {
        public const string ProgId = "EasyZip.Archive";
        public const string ProgIdDescription = "EasyZip 압축 아카이브";

        public static readonly string[] AllSupportedExtensions =
        {
            ".zip", ".7z", ".rar", ".tar", ".gz", ".tgz",
            ".bz2", ".tbz2", ".xz", ".txz", ".iso", ".cab",
            ".zst", ".tzst", ".lz", ".lzh", ".alz", ".egg",
            ".001"
        };

        private static string GetExecutablePath()
        {
            return Process.GetCurrentProcess().MainModule?.FileName 
                   ?? Environment.ProcessPath 
                   ?? Path.Combine(AppDomain.CurrentDomain.BaseDirectory, "EasyZip.exe");
        }

        /// <summary>
        /// ProgID (EasyZip.Archive) 및 Windows 10/11 공식 Capabilities 등록
        /// </summary>
        public static void RegisterProgId()
        {
            string exePath = GetExecutablePath();

            // 1. HKCU\Software\Classes\EasyZip.Archive
            using (var progIdKey = Registry.CurrentUser.CreateSubKey($@"Software\Classes\{ProgId}"))
            {
                if (progIdKey != null)
                {
                    progIdKey.SetValue("", ProgIdDescription);
                    progIdKey.SetValue("FriendlyTypeName", ProgIdDescription);

                    using var iconKey = progIdKey.CreateSubKey("DefaultIcon");
                    iconKey?.SetValue("", $"\"{exePath}\",0");

                    using var commandKey = progIdKey.CreateSubKey(@"shell\open\command");
                    commandKey?.SetValue("", $"\"{exePath}\" \"%1\"");
                }
            }

            // 2. Windows 10/11 RegisteredApplications 및 Capabilities 공식 등록
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
                        foreach (var ext in AllSupportedExtensions)
                        {
                            fileAssocKey.SetValue(ext, ProgId);
                        }
                    }
                }
            }
            catch { }

            // 3. 관리자 권한 실행 시 HKLM RegisteredApplications에도 등록 시도 (Windows 11 ms-settings:defaultapps 완전 호환)
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
                        foreach (var ext in AllSupportedExtensions)
                        {
                            fileAssocKey.SetValue(ext, ProgId);
                        }
                    }
                }
            }
            catch { }
        }

        /// <summary>
        /// 특정 확장자가 실제로 EasyZip으로 열리는지 정밀 확인 (Windows 10/11 UserChoice 완벽 검사)
        /// </summary>
        public static bool IsAssociated(string extension, out string currentHandler)
        {
            if (!extension.StartsWith('.')) extension = "." + extension;
            currentHandler = string.Empty;

            try
            {
                // 1. Windows 10/11 실제 최우선 순위: FileExts\<ext>\UserChoice 검사
                using var userChoiceKey = Registry.CurrentUser.OpenSubKey($@"Software\Microsoft\Windows\CurrentVersion\Explorer\FileExts\{extension}\UserChoice");
                if (userChoiceKey != null)
                {
                    var progIdVal = userChoiceKey.GetValue("ProgId") as string;
                    if (!string.IsNullOrEmpty(progIdVal))
                    {
                        if (progIdVal.Equals(ProgId, StringComparison.OrdinalIgnoreCase))
                        {
                            currentHandler = "EasyZip";
                            return true;
                        }
                        else
                        {
                            currentHandler = GetFriendlyHandlerName(progIdVal);
                            return false; // 다른 프로그램(예: KingZip, Bandizip 등)이 가로채고 있음!
                        }
                    }
                }

                // 2. HKCU\Software\Classes\<ext> 검사
                using var extKey = Registry.CurrentUser.OpenSubKey($@"Software\Classes\{extension}");
                if (extKey != null)
                {
                    var val = extKey.GetValue("") as string;
                    if (!string.IsNullOrEmpty(val))
                    {
                        if (val.Equals(ProgId, StringComparison.OrdinalIgnoreCase))
                        {
                            currentHandler = "EasyZip";
                            return true;
                        }
                        currentHandler = GetFriendlyHandlerName(val);
                        return false;
                    }
                }

                // 3. HKLM / HKCR 시스템 기본값 검사
                using var hkcrKey = Registry.ClassesRoot.OpenSubKey(extension);
                if (hkcrKey != null)
                {
                    var val = hkcrKey.GetValue("") as string;
                    if (!string.IsNullOrEmpty(val))
                    {
                        if (val.Equals(ProgId, StringComparison.OrdinalIgnoreCase))
                        {
                            currentHandler = "EasyZip";
                            return true;
                        }
                        currentHandler = GetFriendlyHandlerName(val);
                    }
                }
            }
            catch { }

            return false;
        }

        public static bool IsAssociated(string extension)
        {
            return IsAssociated(extension, out _);
        }

        private static string GetFriendlyHandlerName(string progId)
        {
            if (string.IsNullOrEmpty(progId)) return "알 수 없음";
            if (progId.Contains("KingZip", StringComparison.OrdinalIgnoreCase)) return "KingZip";
            if (progId.Contains("Bandizip", StringComparison.OrdinalIgnoreCase)) return "반디집";
            if (progId.Contains("ALZip", StringComparison.OrdinalIgnoreCase)) return "알집";
            if (progId.Contains("7-Zip", StringComparison.OrdinalIgnoreCase) || progId.Contains("7z", StringComparison.OrdinalIgnoreCase)) return "7-Zip";
            if (progId.Contains("WinRAR", StringComparison.OrdinalIgnoreCase)) return "WinRAR";
            if (progId.Contains("CompressedFolder", StringComparison.OrdinalIgnoreCase)) return "Windows 탐색기";
            return progId;
        }

        /// <summary>
        /// 특정 확장자를 EasyZip에 연결 및 UserChoice 리셋 시도
        /// </summary>
        public static void AssociateExtension(string extension)
        {
            if (!extension.StartsWith('.')) extension = "." + extension;
            RegisterProgId();

            string exePath = GetExecutablePath();

            // 1. HKCU\Software\Classes\<ext>
            using (var extKey = Registry.CurrentUser.CreateSubKey($@"Software\Classes\{extension}"))
            {
                if (extKey != null)
                {
                    extKey.SetValue("", ProgId);

                    using var openWithKey = extKey.CreateSubKey("OpenWithProgids");
                    openWithKey?.SetValue(ProgId, string.Empty);
                }
            }

            // 2. OpenWithList 및 Applications 등록
            using (var openWithList = Registry.CurrentUser.CreateSubKey($@"Software\Classes\{extension}\OpenWithList\EasyZip.exe")) { }

            using (var appKey = Registry.CurrentUser.CreateSubKey(@"Software\Classes\Applications\EasyZip.exe\shell\open\command"))
            {
                appKey?.SetValue("", $"\"{exePath}\" \"%1\"");
            }

            // 3. FileExts OpenWithProgids 및 UserChoice 리셋 시도
            try
            {
                using var fileExtKey = Registry.CurrentUser.CreateSubKey($@"Software\Microsoft\Windows\CurrentVersion\Explorer\FileExts\{extension}");
                if (fileExtKey != null)
                {
                    using var openWithProgids = fileExtKey.CreateSubKey("OpenWithProgids");
                    openWithProgids?.SetValue(ProgId, new byte[0], RegistryValueKind.None);

                    TryDeleteUserChoice(extension);
                }
            }
            catch { }
        }

        private static void TryDeleteUserChoice(string extension)
        {
            try
            {
                string subKeyPath = $@"Software\Microsoft\Windows\CurrentVersion\Explorer\FileExts\{extension}\UserChoice";
                Registry.CurrentUser.DeleteSubKeyTree(subKeyPath, false);
            }
            catch { }
        }

        public static void UnassociateExtension(string extension)
        {
            if (!extension.StartsWith('.')) extension = "." + extension;
            try
            {
                using var extKey = Registry.CurrentUser.OpenSubKey($@"Software\Classes\{extension}", true);
                if (extKey != null)
                {
                    var val = extKey.GetValue("") as string;
                    if (val == ProgId)
                    {
                        extKey.DeleteValue("", false);
                    }

                    using var openWithKey = extKey.OpenSubKey("OpenWithProgids", true);
                    openWithKey?.DeleteValue(ProgId, false);
                }

                using var fileExtOpenWith = Registry.CurrentUser.OpenSubKey($@"Software\Microsoft\Windows\CurrentVersion\Explorer\FileExts\{extension}\OpenWithProgids", true);
                fileExtOpenWith?.DeleteValue(ProgId, false);
            }
            catch { }
        }

        public static void AssociateAll(IEnumerable<string>? extensions = null)
        {
            extensions ??= AllSupportedExtensions;
            foreach (var ext in extensions)
            {
                AssociateExtension(ext);
            }
            NativeMethods.NotifyShellAssociationChanged();
        }

        public static void UnassociateAll(IEnumerable<string>? extensions = null)
        {
            extensions ??= AllSupportedExtensions;
            foreach (var ext in extensions)
            {
                UnassociateExtension(ext);
            }
            NativeMethods.NotifyShellAssociationChanged();
        }

        /// <summary>
        /// Windows 11 / 10 기본 앱 설정 페이지 직접 열기
        /// </summary>
        public static void OpenWindowsDefaultAppsSettings()
        {
            string[] uris =
            {
                "ms-settings:defaultapps?registeredAppMachine=EasyZip",
                "ms-settings:defaultapps?registeredAppUser=EasyZip",
                "ms-settings:defaultapps"
            };

            foreach (var uri in uris)
            {
                try
                {
                    Process.Start(new ProcessStartInfo
                    {
                        FileName = uri,
                        UseShellExecute = true
                    });
                    return;
                }
                catch { }
            }
        }
    }
}
