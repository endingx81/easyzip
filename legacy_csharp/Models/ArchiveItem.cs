using System;
using System.Windows.Media;
using EasyZip.Services;

namespace EasyZip.Models
{
    public class ArchiveItem
    {
        public string Name { get; set; } = string.Empty;
        public string FullPath { get; set; } = string.Empty;
        public bool IsDirectory { get; set; }
        public long Size { get; set; }
        public long CompressedSize { get; set; }
        public DateTime? LastModified { get; set; }
        public string Crc { get; set; } = string.Empty;
        public string IconKind => IsDirectory ? "📁" : GetFileIcon(Name);
        public ImageSource FileIcon => IsDirectory ? SystemIconHelper.GetFolderIcon() : SystemIconHelper.GetFileIcon(Name);

        public string FormattedSize => IsDirectory ? "" : FormatBytes(Size);
        public string FormattedCompressedSize => IsDirectory ? "" : FormatBytes(CompressedSize);
        public string CompressionRatio
        {
            get
            {
                if (IsDirectory || Size <= 0) return "";
                double ratio = (1.0 - (double)CompressedSize / Size) * 100.0;
                return $"{Math.Max(0, ratio):F1}%";
            }
        }

        public static string FormatBytes(long bytes)
        {
            if (bytes < 0) return "0 B";
            string[] sizes = { "B", "KB", "MB", "GB", "TB" };
            double len = bytes;
            int order = 0;
            while (len >= 1024 && order < sizes.Length - 1)
            {
                order++;
                len /= 1024;
            }
            return $"{len:0.##} {sizes[order]}";
        }

        private static string GetFileIcon(string filename)
        {
            string ext = System.IO.Path.GetExtension(filename).ToLowerInvariant();
            return ext switch
            {
                ".zip" or ".7z" or ".rar" or ".tar" or ".gz" or ".bz2" or ".xz" => "🗜️",
                ".exe" or ".msi" or ".bat" or ".cmd" or ".ps1" => "⚙️",
                ".txt" or ".md" or ".log" or ".json" or ".xml" or ".yaml" => "📄",
                ".jpg" or ".jpeg" or ".png" or ".gif" or ".bmp" or ".webp" => "🖼️",
                ".mp4" or ".mkv" or ".avi" or ".mov" or ".wmv" => "🎬",
                ".mp3" or ".wav" or ".flac" or ".m4a" or ".ogg" => "🎵",
                ".pdf" => "📕",
                ".doc" or ".docx" => "📘",
                ".xls" or ".xlsx" or ".csv" => "📊",
                ".ppt" or ".pptx" => "📙",
                ".iso" or ".img" => "💿",
                _ => "📄"
            };
        }
    }
}

