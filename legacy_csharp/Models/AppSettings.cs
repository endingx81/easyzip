using System;
using System.Collections.Generic;
using System.IO;
using System.Text.Json;

namespace EasyZip.Models
{
    public class AppSettings
    {
        public bool ContextMenuExtractHere { get; set; } = true;
        public bool ContextMenuExtractToFolder { get; set; } = true;
        public bool ContextMenuOpenWithEasyZip { get; set; } = true;
        public bool ContextMenuCompress { get; set; } = true;
        public bool ContextMenuNewFolder { get; set; } = true;

        public List<string> AssociatedExtensions { get; set; } = new()
        {
            ".zip", ".7z", ".rar", ".tar", ".gz", ".tgz",
            ".bz2", ".tbz2", ".xz", ".txz", ".iso", ".cab", ".zst", ".alz", ".egg"
        };

        public bool MultiThreadExtraction { get; set; } = true;
        public int ThreadCount { get; set; } = Environment.ProcessorCount;
        public int BufferSizeKb { get; set; } = 1024; // 1MB 고속 버퍼
        public bool OpenFolderAfterExtraction { get; set; } = false;
        public bool OpenFolderAfterCompression { get; set; } = false;

        private static readonly string SettingsFilePath = Path.Combine(
            Environment.GetFolderPath(Environment.SpecialFolder.ApplicationData),
            "EasyZip",
            "settings.json"
        );

        public static AppSettings Load()
        {
            try
            {
                if (File.Exists(SettingsFilePath))
                {
                    string json = File.ReadAllText(SettingsFilePath);
                    var settings = JsonSerializer.Deserialize<AppSettings>(json);
                    if (settings != null) return settings;
                }
            }
            catch
            {
                // Ignore and return default
            }
            return new AppSettings();
        }

        public void Save()
        {
            try
            {
                string dir = Path.GetDirectoryName(SettingsFilePath)!;
                if (!Directory.Exists(dir)) Directory.CreateDirectory(dir);
                string json = JsonSerializer.Serialize(this, new JsonSerializerOptions { WriteIndented = true });
                File.WriteAllText(SettingsFilePath, json);
            }
            catch
            {
                // Ignore
            }
        }
    }
}

