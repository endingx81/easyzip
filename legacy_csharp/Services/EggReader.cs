using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Threading;
using System.Threading.Tasks;
using EasyZip.Models;
using EggDotNet;

namespace EasyZip.Services
{
    public static class EggReader
    {
        public static bool IsEggFile(string filePath)
        {
            try
            {
                using var fs = new FileStream(filePath, FileMode.Open, FileAccess.Read, FileShare.Read);
                if (fs.Length < 4) return false;
                byte[] magic = new byte[4];
                fs.Read(magic, 0, 4);
                // EGG 매직: 0x41474745 (EGGA)
                return magic[0] == 0x45 && magic[1] == 0x47 && magic[2] == 0x47 && magic[3] == 0x41;
            }
            catch
            {
                return false;
            }
        }

        public static List<ArchiveItem> GetEntries(string filePath)
        {
            var items = new List<ArchiveItem>();
            using var egg = EggFile.Open(filePath);

            foreach (var entry in egg.Entries)
            {
                string rawPath = entry.FullName ?? entry.Name ?? "item";
                string safePath = EncodingHelper.SanitizePath(rawPath);
                bool isDir = rawPath.EndsWith('/') || rawPath.EndsWith('\\') || safePath.EndsWith('/');
                safePath = safePath.TrimEnd('/', '\\');

                items.Add(new ArchiveItem
                {
                    Name = Path.GetFileName(safePath),
                    FullPath = safePath,
                    IsDirectory = isDir,
                    Size = isDir ? 0 : entry.UncompressedLength,
                    CompressedSize = isDir ? 0 : entry.CompressedLength,
                    LastModified = entry.LastWriteTime,
                    Crc = entry.Crc32.ToString("X8")
                });
            }

            return items;
        }

        public static async Task ExtractAllAsync(
            string eggPath,
            string destinationDirectory,
            IProgress<ExtractionProgressInfo>? progress = null,
            CancellationToken cancellationToken = default)
        {
            await Task.Run(() =>
            {
                using var egg = EggFile.Open(eggPath);

                long totalBytes = 0;
                foreach (var e in egg.Entries)
                {
                    string rawPath = e.FullName ?? e.Name ?? "";
                    bool isDir = rawPath.EndsWith('/') || rawPath.EndsWith('\\');
                    if (!isDir) totalBytes += e.UncompressedLength;
                }

                long processedBytes = 0;
                int processedFiles = 0;
                var stopwatch = Stopwatch.StartNew();

                // 디렉터리 사전 일괄 생성
                foreach (var e in egg.Entries)
                {
                    string rawPath = e.FullName ?? e.Name ?? "item";
                    bool isDir = rawPath.EndsWith('/') || rawPath.EndsWith('\\');
                    string safePath = EncodingHelper.SanitizePath(rawPath);
                    string fullPath = Path.GetFullPath(Path.Combine(destinationDirectory, safePath));
                    if (!fullPath.StartsWith(Path.GetFullPath(destinationDirectory), StringComparison.OrdinalIgnoreCase))
                        continue;

                    string? parentDir = isDir ? fullPath : Path.GetDirectoryName(fullPath);
                    if (!string.IsNullOrEmpty(parentDir) && !Directory.Exists(parentDir))
                    {
                        Directory.CreateDirectory(parentDir);
                    }
                }

                byte[] buffer = new byte[1024 * 512];

                foreach (var entry in egg.Entries)
                {
                    cancellationToken.ThrowIfCancellationRequested();

                    string rawPath = entry.FullName ?? entry.Name ?? "item";
                    bool isDir = rawPath.EndsWith('/') || rawPath.EndsWith('\\');
                    if (isDir) continue;

                    string safePath = EncodingHelper.SanitizePath(rawPath);
                    string fullPath = Path.GetFullPath(Path.Combine(destinationDirectory, safePath));
                    if (!fullPath.StartsWith(Path.GetFullPath(destinationDirectory), StringComparison.OrdinalIgnoreCase))
                        continue;

                    using (var entryStream = entry.Open())
                    using (var outStream = new FileStream(fullPath, FileMode.Create, FileAccess.Write, FileShare.None, 1024 * 512))
                    {
                        int read;
                        while ((read = entryStream.Read(buffer, 0, buffer.Length)) > 0)
                        {
                            cancellationToken.ThrowIfCancellationRequested();
                            outStream.Write(buffer, 0, read);
                            processedBytes += read;

                            ReportProgress(progress, Path.GetFileName(fullPath), processedBytes, totalBytes,
                                processedFiles, egg.Entries.Count, stopwatch);
                        }
                    }

                    processedFiles++;
                    ReportProgress(progress, Path.GetFileName(fullPath), processedBytes, totalBytes,
                        processedFiles, egg.Entries.Count, stopwatch);
                }

                stopwatch.Stop();
                progress?.Report(new ExtractionProgressInfo
                {
                    CurrentFile = "완료",
                    ProcessedBytes = totalBytes,
                    TotalBytes = totalBytes,
                    ProcessedFiles = processedFiles,
                    TotalFiles = egg.Entries.Count,
                    Percentage = 100.0,
                    SpeedBytesPerSec = totalBytes / Math.Max(0.001, stopwatch.Elapsed.TotalSeconds),
                    RemainingTime = TimeSpan.Zero,
                    StatusMessage = $"EGG 압축 풀기 완료 ({stopwatch.Elapsed.TotalSeconds:F2}초 소요)"
                });
            }, cancellationToken);
        }

        public static async Task<string> ExtractSingleFileAsync(string eggPath, string entryFullPath)
        {
            return await Task.Run(() =>
            {
                using var egg = EggFile.Open(eggPath);
                EggArchiveEntry? target = null;

                string normalizedTarget = entryFullPath.Replace('\\', '/').Trim('/');

                foreach (var entry in egg.Entries)
                {
                    string safe = EncodingHelper.SanitizePath(entry.FullName ?? entry.Name ?? "").Replace('\\', '/').Trim('/');
                    if (safe.Equals(normalizedTarget, StringComparison.OrdinalIgnoreCase) ||
                        Path.GetFileName(safe).Equals(Path.GetFileName(normalizedTarget), StringComparison.OrdinalIgnoreCase))
                    {
                        target = entry;
                        break;
                    }
                }

                if (target == null) throw new FileNotFoundException($"EGG 내부에서 '{entryFullPath}'를 찾을 수 없습니다.");

                string tempDir = Path.Combine(Path.GetTempPath(), "EasyZip_Temp", Path.GetRandomFileName());
                Directory.CreateDirectory(tempDir);
                string outPath = Path.Combine(tempDir, Path.GetFileName(target.FullName ?? target.Name ?? "extracted_file"));

                using var es = target.Open();
                using var fs = File.Create(outPath);
                es.CopyTo(fs);

                return outPath;
            });
        }

        private static void ReportProgress(
            IProgress<ExtractionProgressInfo>? progress,
            string currentFile,
            long processedBytes,
            long totalBytes,
            int processedFiles,
            int totalFiles,
            Stopwatch stopwatch)
        {
            if (progress == null) return;

            double elapsedSec = stopwatch.Elapsed.TotalSeconds;
            double speed = elapsedSec > 0.05 ? processedBytes / elapsedSec : 0;
            double pct = totalBytes > 0 ? Math.Min(100.0, (double)processedBytes / totalBytes * 100.0) : 0;

            TimeSpan? remaining = null;
            if (speed > 1024 && totalBytes > processedBytes)
            {
                double remainingSec = (totalBytes - processedBytes) / speed;
                if (remainingSec < 86400) remaining = TimeSpan.FromSeconds(remainingSec);
            }

            progress.Report(new ExtractionProgressInfo
            {
                CurrentFile = currentFile,
                ProcessedBytes = processedBytes,
                TotalBytes = totalBytes,
                ProcessedFiles = processedFiles,
                TotalFiles = totalFiles,
                Percentage = pct,
                SpeedBytesPerSec = speed,
                RemainingTime = remaining,
                StatusMessage = $"EGG 압축 해제 중... ({processedFiles}/{totalFiles} 파일, {pct:F1}%)"
            });
        }
    }
}
