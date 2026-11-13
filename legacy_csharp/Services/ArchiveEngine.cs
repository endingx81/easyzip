using System;
using System.Collections.Concurrent;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.IO.Compression;
using System.Linq;
using System.Threading;
using System.Threading.Tasks;
using EasyZip.Models;
using SharpCompress.Archives;
using SharpCompress.Common;
using SharpCompress.Readers;

namespace EasyZip.Services
{
    public class ArchiveEngine
    {
        private const int DefaultBufferSize = 1024 * 1024; // 1MB 고속 스트림 버퍼

        /// <summary>
        /// 지원되는 압축 파일인지 확인
        /// </summary>
        public static bool IsSupportedArchive(string path)
        {
            if (string.IsNullOrEmpty(path)) return false;
            string ext = Path.GetExtension(path).ToLowerInvariant();
            return FileAssociation.AllSupportedExtensions.Contains(ext) || MultiVolumeStream.IsSplitVolume(path);
        }

        /// <summary>
        /// 아카이브 내부 파일/디렉터리 목록 조회 (초고속 메타데이터 분석)
        /// </summary>
        public static Task<List<ArchiveItem>> GetArchiveEntriesAsync(string archivePath, System.Text.Encoding? preferredEncoding = null)
        {
            EncodingHelper.Initialize();

            return Task.Run(() =>
            {
                var items = new List<ArchiveItem>();
                string ext = Path.GetExtension(archivePath).ToLowerInvariant();

                // 0. 분할 볼륨 아카이브 (.001, .7z.001, .zip.001 등)
                if (MultiVolumeStream.IsSplitVolume(archivePath))
                {
                    try
                    {
                        using var mvs = new MultiVolumeStream(archivePath);
                        using var archive = ArchiveFactory.OpenArchive(mvs);
                        foreach (var entry in archive.Entries)
                        {
                            bool isDir = entry.IsDirectory;
                            string key = entry.Key?.Replace('\\', '/') ?? "";
                            string cleanName = key.TrimEnd('/');
                            string itemName = Path.GetFileName(cleanName);
                            if (string.IsNullOrEmpty(itemName)) itemName = cleanName;

                            string crcStr = "";
                            long compSize = 0;
                            long uncompSize = 0;
                            if (!isDir)
                            {
                                try { crcStr = entry.Crc.ToString("X8"); } catch { crcStr = ""; }
                                try { compSize = entry.CompressedSize; } catch { compSize = 0; }
                                try { uncompSize = entry.Size; } catch { uncompSize = 0; }
                            }

                            DateTime? lastMod = null;
                            try { lastMod = entry.LastModifiedTime; } catch { }

                            items.Add(new ArchiveItem
                            {
                                Name = itemName,
                                FullPath = key,
                                IsDirectory = isDir,
                                Size = uncompSize,
                                CompressedSize = compSize,
                                LastModified = lastMod,
                                Crc = crcStr
                            });
                        }
                        return items;
                    }
                    catch
                    {
                        // 아카이브 파싱이 안 되는 원시 분할 파일인 경우 (예: movie.iso.001 -> movie.iso)
                        using var mvs = new MultiVolumeStream(archivePath);
                        string joinedName = Path.GetFileNameWithoutExtension(archivePath);
                        if (string.IsNullOrEmpty(joinedName) || joinedName.StartsWith('.'))
                            joinedName = "CombinedFile";

                        items.Add(new ArchiveItem
                        {
                            Name = joinedName,
                            FullPath = joinedName,
                            IsDirectory = false,
                            Size = mvs.Length,
                            CompressedSize = mvs.Length,
                            LastModified = File.GetLastWriteTime(archivePath)
                        });
                        return items;
                    }
                }

                // 1. ALZ 포맷 (알집)
                if (ext == ".alz" || AlzReader.IsAlzFile(archivePath))
                {
                    var alzEntries = AlzReader.ReadEntries(archivePath, preferredEncoding);
                    foreach (var entry in alzEntries)
                    {
                        items.Add(new ArchiveItem
                        {
                            Name = entry.Name,
                            FullPath = entry.FullPath,
                            IsDirectory = entry.IsDirectory,
                            Size = entry.UncompressedSize,
                            CompressedSize = entry.CompressedSize,
                            LastModified = entry.LastModified,
                            Crc = entry.Crc32.ToString("X8")
                        });
                    }
                    return items;
                }

                // 2. EGG 포맷 (알집)
                if (ext == ".egg" || EggReader.IsEggFile(archivePath))
                {
                    return EggReader.GetEntries(archivePath);
                }

                // 3. ZIP 파일인 경우 다국어/한국어 인코딩 자동 보정 리더 사용
                if (ext == ".zip")
                {
                    try
                    {
                        var enc = preferredEncoding ?? EncodingHelper.KoreanEncoding;
                        using var zip = System.IO.Compression.ZipFile.Open(archivePath, ZipArchiveMode.Read, enc);
                        foreach (var entry in zip.Entries)
                        {
                            string safePath = EncodingHelper.SanitizePath(entry.FullName);
                            bool isDir = entry.FullName.EndsWith("/") || entry.FullName.EndsWith("\\") || safePath.EndsWith('/');
                            safePath = safePath.TrimEnd('/', '\\');
                            string itemName = Path.GetFileName(safePath);
                            if (string.IsNullOrEmpty(itemName)) itemName = safePath;

                            items.Add(new ArchiveItem
                            {
                                Name = itemName,
                                FullPath = safePath,
                                IsDirectory = isDir,
                                Size = isDir ? 0 : entry.Length,
                                CompressedSize = isDir ? 0 : entry.CompressedLength,
                                LastModified = entry.LastWriteTime.LocalDateTime,
                                Crc = entry.Crc32.ToString("X8")
                            });
                        }
                        return items;
                    }
                    catch
                    {
                        // ZIP 파싱 실패 시 아래 SharpCompress로 폴백
                    }
                }

                // 4. TAR / TAR.GZ / TGZ / GZ 아카이브 처리
                if (ext == ".gz" || ext == ".tgz" || archivePath.EndsWith(".tar.gz", StringComparison.OrdinalIgnoreCase) || archivePath.EndsWith(".tar_2.gz", StringComparison.OrdinalIgnoreCase) || ext == ".tar")
                {
                    try
                    {
                        using var fs = new FileStream(archivePath, FileMode.Open, FileAccess.Read, FileShare.Read, DefaultBufferSize, FileOptions.SequentialScan);
                        Stream stream = fs;
                        System.IO.Compression.GZipStream? gzStream = null;

                        if (ext == ".gz" || ext == ".tgz" || archivePath.Contains(".tar", StringComparison.OrdinalIgnoreCase))
                        {
                            gzStream = new System.IO.Compression.GZipStream(fs, System.IO.Compression.CompressionMode.Decompress);
                            stream = gzStream;
                        }

                        using var reader = SharpCompress.Readers.ReaderFactory.OpenReader(stream);
                        while (reader.MoveToNextEntry())
                        {
                            var entry = reader.Entry;
                            bool isDir = entry.IsDirectory;
                            string key = entry.Key?.Replace('\\', '/') ?? "";
                            string cleanName = key.TrimEnd('/');
                            string itemName = Path.GetFileName(cleanName);
                            if (string.IsNullOrEmpty(itemName)) itemName = cleanName;

                            items.Add(new ArchiveItem
                            {
                                Name = itemName,
                                FullPath = key,
                                IsDirectory = isDir,
                                Size = isDir ? 0 : entry.Size,
                                CompressedSize = 0,
                                LastModified = entry.LastModifiedTime,
                                Crc = ""
                            });
                        }

                        gzStream?.Dispose();
                        if (items.Count > 0) return items;
                    }
                    catch (Exception ex)
                    {
                        Console.WriteLine("[TarReader debug error]: " + ex.Message);
                    }
                }

                // 5. SharpCompress 기반 범용 아카이브 오픈 (7z, RAR, TAR, GZ, ISO, CAB 등)
                using (var archive = ArchiveFactory.OpenArchive(archivePath))
                {
                    using var enumerator = archive.Entries.GetEnumerator();
                    while (true)
                    {
                        bool hasNext;
                        try
                        {
                            hasNext = enumerator.MoveNext();
                        }
                        catch (Exception ex)
                        {
                            Console.WriteLine("[ArchiveEngine warning] Entry enumeration failed: " + ex.Message);
                            break;
                        }

                        if (!hasNext) break;

                        var entry = enumerator.Current;
                        if (entry == null) continue;

                        bool isDir = false;
                        try { isDir = entry.IsDirectory; } catch { }

                        string key = "";
                        try { key = entry.Key?.Replace('\\', '/') ?? ""; } catch { }
                        string cleanName = key.TrimEnd('/');
                        string itemName = Path.GetFileName(cleanName);
                        if (string.IsNullOrEmpty(itemName)) itemName = cleanName;

                        string crcStr = "";
                        long compSize = 0;
                        long uncompSize = 0;
                        if (!isDir)
                        {
                            try { crcStr = entry.Crc.ToString("X8"); } catch { crcStr = ""; }
                            try { compSize = entry.CompressedSize; } catch { compSize = 0; }
                            try { uncompSize = entry.Size; } catch { uncompSize = 0; }
                        }

                        DateTime? lastMod = null;
                        try { lastMod = entry.LastModifiedTime; } catch { }

                        items.Add(new ArchiveItem
                        {
                            Name = itemName,
                            FullPath = key,
                            IsDirectory = isDir,
                            Size = uncompSize,
                            CompressedSize = compSize,
                            LastModified = lastMod,
                            Crc = crcStr
                        });
                    }
                }

                return items;
            });
        }

        /// <summary>
        /// 초고속 아카이브 압축 해제 (멀티스레딩 & 대용량 I/O 버퍼 최적화)
        /// </summary>
        public static async Task ExtractArchiveAsync(
            string archivePath,
            string destinationDirectory,
            IProgress<ExtractionProgressInfo>? progress = null,
            CancellationToken cancellationToken = default,
            int maxThreads = 0)
        {
            if (!File.Exists(archivePath))
                throw new FileNotFoundException("압축 파일을 찾을 수 없습니다.", archivePath);

            if (!Directory.Exists(destinationDirectory))
                Directory.CreateDirectory(destinationDirectory);

            string ext = Path.GetExtension(archivePath).ToLowerInvariant();

            // 0. 분할 볼륨 아카이브 (.001, .7z.001, .zip.001 등)
            if (MultiVolumeStream.IsSplitVolume(archivePath))
            {
                await ExtractSplitVolumeAsync(archivePath, destinationDirectory, progress, cancellationToken).ConfigureAwait(false);
                return;
            }

            // 1. ALZ 포맷 (알집)
            if (ext == ".alz" || AlzReader.IsAlzFile(archivePath))
            {
                await AlzReader.ExtractAllAsync(archivePath, destinationDirectory, progress, cancellationToken).ConfigureAwait(false);
                return;
            }

            // 2. EGG 포맷 (알집)
            if (ext == ".egg" || EggReader.IsEggFile(archivePath))
            {
                await EggReader.ExtractAllAsync(archivePath, destinationDirectory, progress, cancellationToken).ConfigureAwait(false);
                return;
            }

            // 3. .zip 파일인 경우 멀티스레드 병렬 압축 해제 우선 시도
            if (ext == ".zip")
            {
                bool success = false;
                try
                {
                    await ExtractZipParallelAsync(archivePath, destinationDirectory, progress, cancellationToken, maxThreads).ConfigureAwait(false);
                    success = true;
                }
                catch (InvalidDataException)
                {
                    success = false;
                }

                if (success) return;
            }

            // 4. TAR / TAR.GZ / TGZ / GZ 압축 해제 (.NET 8 내장 TarFile)
            if (ext == ".tar" || ext == ".gz" || ext == ".tgz" || archivePath.EndsWith(".tar.gz", StringComparison.OrdinalIgnoreCase) || archivePath.EndsWith(".tar_2.gz", StringComparison.OrdinalIgnoreCase))
            {
                bool success = false;
                try
                {
                    await ExtractTarGzAsync(archivePath, destinationDirectory, progress, cancellationToken).ConfigureAwait(false);
                    success = true;
                }
                catch
                {
                    success = false;
                }

                if (success) return;
            }

            // 7z, RAR, TAR, GZ, XZ, ISO, CAB 등 SharpCompress 고속 스트림 압축 해제
            await ExtractArchiveSharpCompressAsync(archivePath, destinationDirectory, progress, cancellationToken).ConfigureAwait(false);
        }

        /// <summary>
        /// 초고속 병렬 멀티스레드 ZIP 압축 해제
        /// CPU 코어 수만큼 동시 파일 추출을 수행하여 NVMe / SSD 대역폭을 최대 활용
        /// </summary>
        private static Task ExtractZipParallelAsync(
            string archivePath,
            string destinationDirectory,
            IProgress<ExtractionProgressInfo>? progress,
            CancellationToken cancellationToken,
            int maxThreads)
        {
            return Task.Run(() =>
            {
                int threads = maxThreads > 0 ? maxThreads : Environment.ProcessorCount;

                List<(string FullName, long Length, uint Crc)> entries;
                long totalBytes = 0;
                int totalFiles = 0;

                var enc = EncodingHelper.KoreanEncoding;
                using (var zip = System.IO.Compression.ZipFile.Open(archivePath, ZipArchiveMode.Read, enc))
                {
                    entries = zip.Entries.Select(e => (e.FullName, e.Length, e.Crc32)).ToList();
                    foreach (var e in entries)
                    {
                        if (!e.FullName.EndsWith('/') && !e.FullName.EndsWith('\\'))
                        {
                            totalBytes += e.Length;
                            totalFiles++;
                        }
                    }
                }

                // 필요한 모든 하위 디렉터리 사전 일괄 생성 (스레드 경쟁 상태 원천 차단 & I/O 극대화)
                var allDirs = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
                foreach (var e in entries)
                {
                    string safeName = EncodingHelper.SanitizePath(e.FullName);
                    string fullPath = Path.GetFullPath(Path.Combine(destinationDirectory, safeName));
                    if (!fullPath.StartsWith(Path.GetFullPath(destinationDirectory), StringComparison.OrdinalIgnoreCase))
                        continue;

                    if (e.FullName.EndsWith('/') || e.FullName.EndsWith('\\'))
                    {
                        allDirs.Add(fullPath);
                    }
                    else
                    {
                        string? parent = Path.GetDirectoryName(fullPath);
                        if (!string.IsNullOrEmpty(parent))
                        {
                            allDirs.Add(parent);
                        }
                    }
                }

                foreach (var dir in allDirs)
                {
                    if (!Directory.Exists(dir))
                    {
                        Directory.CreateDirectory(dir);
                    }
                }

                long processedBytes = 0;
                int processedFiles = 0;
                var stopwatch = Stopwatch.StartNew();

                var parallelOptions = new ParallelOptions
                {
                    MaxDegreeOfParallelism = threads,
                    CancellationToken = cancellationToken
                };

                Parallel.ForEach(entries, parallelOptions, (item, state) =>
                {
                    cancellationToken.ThrowIfCancellationRequested();

                    string safeName = EncodingHelper.SanitizePath(item.FullName);
                    string fullPath = Path.GetFullPath(Path.Combine(destinationDirectory, safeName));
                    if (!fullPath.StartsWith(Path.GetFullPath(destinationDirectory), StringComparison.OrdinalIgnoreCase))
                        return;

                    // 디렉터리 엔트리는 이미 사전 생성되었으므로 스킵
                    if (item.FullName.EndsWith('/') || item.FullName.EndsWith('\\'))
                        return;

                    // 파일 단위로 독립 스트림 오픈 후 초고속 디스크 기록
                    using (var fs = new FileStream(archivePath, FileMode.Open, FileAccess.Read, FileShare.Read, DefaultBufferSize, FileOptions.SequentialScan))
                    using (var zipArchive = new System.IO.Compression.ZipArchive(fs, ZipArchiveMode.Read, false, enc))
                    {
                        var entry = zipArchive.GetEntry(item.FullName);
                        if (entry == null) return;

                        using var entryStream = entry.Open();
                        using var outStream = new FileStream(fullPath, FileMode.Create, FileAccess.Write, FileShare.None, DefaultBufferSize, FileOptions.SequentialScan);

                        byte[] buffer = new byte[DefaultBufferSize];
                        int read;

                        while ((read = entryStream.Read(buffer, 0, buffer.Length)) > 0)
                        {
                            cancellationToken.ThrowIfCancellationRequested();
                            outStream.Write(buffer, 0, read);

                            long currentTotal = Interlocked.Add(ref processedBytes, read);
                            ReportProgress(progress, Path.GetFileName(fullPath), currentTotal, totalBytes,
                                Volatile.Read(ref processedFiles), totalFiles, stopwatch);
                        }
                    }

                    int curFiles = Interlocked.Increment(ref processedFiles);
                    ReportProgress(progress, Path.GetFileName(fullPath), Volatile.Read(ref processedBytes),
                        totalBytes, curFiles, totalFiles, stopwatch);
                });

                stopwatch.Stop();
                progress?.Report(new ExtractionProgressInfo
                {
                    CurrentFile = "완료",
                    ProcessedBytes = totalBytes,
                    TotalBytes = totalBytes,
                    ProcessedFiles = totalFiles,
                    TotalFiles = totalFiles,
                    Percentage = 100.0,
                    SpeedBytesPerSec = totalBytes / Math.Max(0.001, stopwatch.Elapsed.TotalSeconds),
                    RemainingTime = TimeSpan.Zero,
                    StatusMessage = $"압축 풀기 완료 ({stopwatch.Elapsed.TotalSeconds:F2}초 소요)"
                });
            }, cancellationToken);
        }

        /// <summary>
        /// TAR 및 TAR.GZ 초고속 스트림 추출 (.NET 8 내장 고성능 TarReader)
        /// </summary>
        private static Task ExtractTarGzAsync(
            string archivePath,
            string destinationDirectory,
            IProgress<ExtractionProgressInfo>? progress,
            CancellationToken cancellationToken)
        {
            return Task.Run(() =>
            {
                string ext = Path.GetExtension(archivePath).ToLowerInvariant();
                using var fs = new FileStream(archivePath, FileMode.Open, FileAccess.Read, FileShare.Read, DefaultBufferSize, FileOptions.SequentialScan);
                Stream stream = fs;
                System.IO.Compression.GZipStream? gzStream = null;

                if (ext == ".gz" || ext == ".tgz" || archivePath.Contains(".tar", StringComparison.OrdinalIgnoreCase))
                {
                    gzStream = new System.IO.Compression.GZipStream(fs, System.IO.Compression.CompressionMode.Decompress);
                    stream = gzStream;
                }

                using var reader = SharpCompress.Readers.ReaderFactory.OpenReader(stream);
                var createdDirs = new ConcurrentDictionary<string, bool>(StringComparer.OrdinalIgnoreCase);
                long processedBytes = 0;
                int processedFiles = 0;
                var stopwatch = Stopwatch.StartNew();

                var extractOptions = new SharpCompress.Common.ExtractionOptions
                {
                    ExtractFullPath = true,
                    Overwrite = true
                };

                while (reader.MoveToNextEntry())
                {
                    cancellationToken.ThrowIfCancellationRequested();
                    var entry = reader.Entry;

                    if (entry.IsDirectory)
                    {
                        string dirPath = Path.GetFullPath(Path.Combine(destinationDirectory, entry.Key ?? ""));
                        EnsureDirectoryCreated(dirPath, createdDirs);
                        continue;
                    }

                    string key = entry.Key ?? Guid.NewGuid().ToString();
                    string fullPath = Path.GetFullPath(Path.Combine(destinationDirectory, key));
                    if (!fullPath.StartsWith(Path.GetFullPath(destinationDirectory), StringComparison.OrdinalIgnoreCase))
                        continue;

                    string? parentDir = Path.GetDirectoryName(fullPath);
                    if (!string.IsNullOrEmpty(parentDir))
                    {
                        EnsureDirectoryCreated(parentDir, createdDirs);
                    }

                    reader.WriteEntryToFile(fullPath, extractOptions);
                    processedBytes += entry.Size;
                    processedFiles++;

                    ReportProgress(progress, Path.GetFileName(fullPath), processedBytes, processedBytes,
                        processedFiles, processedFiles, stopwatch);
                }

                gzStream?.Dispose();
                stopwatch.Stop();
                progress?.Report(new ExtractionProgressInfo
                {
                    CurrentFile = "완료",
                    ProcessedBytes = processedBytes,
                    TotalBytes = processedBytes,
                    ProcessedFiles = processedFiles,
                    TotalFiles = processedFiles,
                    Percentage = 100.0,
                    SpeedBytesPerSec = processedBytes / Math.Max(0.001, stopwatch.Elapsed.TotalSeconds),
                    RemainingTime = TimeSpan.Zero,
                    StatusMessage = $"TAR/GZ 압축 해제 완료 ({stopwatch.Elapsed.TotalSeconds:F2}초 소요)"
                });
            }, cancellationToken);
        }

        /// <summary>
        /// 분할 볼륨 아카이브 해제 또는 분할 파일 병합
        /// </summary>
        private static Task ExtractSplitVolumeAsync(
            string archivePath,
            string destinationDirectory,
            IProgress<ExtractionProgressInfo>? progress,
            CancellationToken cancellationToken)
        {
            return Task.Run(() =>
            {
                using var mvs = new MultiVolumeStream(archivePath);
                try
                {
                    using var archive = ArchiveFactory.OpenArchive(mvs);
                    ExtractArchiveInternal(archive, destinationDirectory, progress, cancellationToken);
                }
                catch
                {
                    // 아카이브 포맷이 아닌 원시 분할 파일 합치기 (예: movie.iso.001 -> movie.iso)
                    mvs.Position = 0;
                    string joinedName = Path.GetFileNameWithoutExtension(archivePath);
                    if (string.IsNullOrEmpty(joinedName) || joinedName.StartsWith('.'))
                        joinedName = "CombinedFile";

                    string destFile = Path.Combine(destinationDirectory, joinedName);
                    byte[] buffer = new byte[DefaultBufferSize];
                    long totalBytes = mvs.Length;
                    long processedBytes = 0;
                    var stopwatch = Stopwatch.StartNew();

                    using (var outStream = new FileStream(destFile, FileMode.Create, FileAccess.Write, FileShare.None, DefaultBufferSize, FileOptions.SequentialScan))
                    {
                        int read;
                        while ((read = mvs.Read(buffer, 0, buffer.Length)) > 0)
                        {
                            cancellationToken.ThrowIfCancellationRequested();
                            outStream.Write(buffer, 0, read);
                            processedBytes += read;
                            ReportProgress(progress, joinedName, processedBytes, totalBytes, 1, 1, stopwatch, "분할 파일 결합 중");
                        }
                    }

                    stopwatch.Stop();
                    progress?.Report(new ExtractionProgressInfo
                    {
                        CurrentFile = "완료",
                        ProcessedBytes = totalBytes,
                        TotalBytes = totalBytes,
                        ProcessedFiles = 1,
                        TotalFiles = 1,
                        Percentage = 100.0,
                        SpeedBytesPerSec = totalBytes / Math.Max(0.001, stopwatch.Elapsed.TotalSeconds),
                        RemainingTime = TimeSpan.Zero,
                        StatusMessage = $"분할 파일 결합 완료 ({stopwatch.Elapsed.TotalSeconds:F2}초)"
                    });
                }
            }, cancellationToken);
        }

        /// <summary>
        /// 7z, RAR, TAR, GZ, XZ, ISO, CAB 등 고성능 스트림 추출
        /// </summary>
        private static Task ExtractArchiveSharpCompressAsync(
            string archivePath,
            string destinationDirectory,
            IProgress<ExtractionProgressInfo>? progress,
            CancellationToken cancellationToken)
        {
            return Task.Run(() =>
            {
                try
                {
                    using var archive = ArchiveFactory.OpenArchive(archivePath);
                    ExtractArchiveInternal(archive, destinationDirectory, progress, cancellationToken);
                }
                catch (Exception ex) when (IsSeekOrStreamEofException(ex))
                {
                    // 아카이브 파일의 뒤쪽이 잘렸거나(다운로드 중단 등) 인덱스 Seek 실패 시,
                    // 순차 스트림 리더(ReaderFactory)로 폴백하여 손상되지 않은 앞쪽 엔트리들을 최대한 안전 복구 추출
                    ExtractWithStreamingReader(archivePath, destinationDirectory, progress, cancellationToken, ex);
                }
            }, cancellationToken);
        }

        private static bool IsSeekOrStreamEofException(Exception ex)
        {
            string msg = ex.ToString();
            return msg.Contains("Cannot seek to position", StringComparison.OrdinalIgnoreCase) ||
                   msg.Contains("End of stream reached", StringComparison.OrdinalIgnoreCase) ||
                   msg.Contains("does not match header", StringComparison.OrdinalIgnoreCase) ||
                   ex is EndOfStreamException;
        }

        private static void ExtractWithStreamingReader(
            string archivePath,
            string destinationDirectory,
            IProgress<ExtractionProgressInfo>? progress,
            CancellationToken cancellationToken,
            Exception originalException)
        {
            var createdDirs = new ConcurrentDictionary<string, bool>(StringComparer.OrdinalIgnoreCase);
            long processedBytes = 0;
            int processedFiles = 0;
            var stopwatch = Stopwatch.StartNew();
            byte[] buffer = new byte[DefaultBufferSize];

            using var fs = new FileStream(archivePath, FileMode.Open, FileAccess.Read, FileShare.Read, DefaultBufferSize, FileOptions.SequentialScan);
            using var reader = SharpCompress.Readers.ReaderFactory.OpenReader(fs);

            string? lastFailedFile = null;
            Exception? streamException = null;

            while (true)
            {
                cancellationToken.ThrowIfCancellationRequested();

                bool hasNext = false;
                try
                {
                    hasNext = reader.MoveToNextEntry();
                }
                catch (Exception ex)
                {
                    streamException = ex;
                    break;
                }

                if (!hasNext) break;

                var entry = reader.Entry;
                if (entry.IsDirectory)
                {
                    string dirPath = Path.GetFullPath(Path.Combine(destinationDirectory, entry.Key ?? ""));
                    EnsureDirectoryCreated(dirPath, createdDirs);
                    continue;
                }

                string key = entry.Key ?? Guid.NewGuid().ToString();
                string fullPath = Path.GetFullPath(Path.Combine(destinationDirectory, key));
                if (!fullPath.StartsWith(Path.GetFullPath(destinationDirectory), StringComparison.OrdinalIgnoreCase))
                    continue;

                string? parentDir = Path.GetDirectoryName(fullPath);
                if (!string.IsNullOrEmpty(parentDir))
                {
                    EnsureDirectoryCreated(parentDir, createdDirs);
                }

                try
                {
                    using (var entryStream = reader.OpenEntryStream())
                    using (var outStream = new FileStream(fullPath, FileMode.Create, FileAccess.Write, FileShare.None, DefaultBufferSize, FileOptions.SequentialScan))
                    {
                        int read;
                        while ((read = entryStream.Read(buffer, 0, buffer.Length)) > 0)
                        {
                            cancellationToken.ThrowIfCancellationRequested();
                            outStream.Write(buffer, 0, read);
                            processedBytes += read;
                            ReportProgress(progress, Path.GetFileName(fullPath), processedBytes, 0,
                                processedFiles, 0, stopwatch, "스트림 복구 압축 해제 중");
                        }
                    }
                    processedFiles++;
                }
                catch (Exception ex)
                {
                    lastFailedFile = Path.GetFileName(fullPath);
                    streamException = ex;
                    break;
                }
            }

            stopwatch.Stop();

            if (streamException != null)
            {
                throw new InvalidDataException(
                    $"파일 뒷부분이 잘린 불완전한 압축 파일입니다.\n\n" +
                    (processedFiles > 0 ? $"• 정상 추출된 파일: {processedFiles}개 완료\n" : "") +
                    (lastFailedFile != null ? $"• 손상/중단된 파일: '{lastFailedFile}'\n" : "") +
                    $"• 상세 원인: {streamException.Message}",
                    streamException);
            }

            progress?.Report(new ExtractionProgressInfo
            {
                CurrentFile = "완료",
                ProcessedBytes = processedBytes,
                TotalBytes = processedBytes,
                ProcessedFiles = processedFiles,
                TotalFiles = processedFiles,
                Percentage = 100.0,
                SpeedBytesPerSec = processedBytes / Math.Max(0.001, stopwatch.Elapsed.TotalSeconds),
                RemainingTime = TimeSpan.Zero,
                StatusMessage = $"압축 풀기 완료 ({processedFiles}개 파일)"
            });
        }

        private static void ExtractArchiveInternal(
            IArchive archive,
            string destinationDirectory,
            IProgress<ExtractionProgressInfo>? progress,
            CancellationToken cancellationToken)
        {
            var createdDirs = new ConcurrentDictionary<string, bool>(StringComparer.OrdinalIgnoreCase);
            long totalBytes = 0;
            int totalFiles = 0;

            foreach (var entry in archive.Entries)
            {
                if (!entry.IsDirectory)
                {
                    try { totalBytes += entry.Size; } catch { }
                    totalFiles++;
                }
            }

            long processedBytes = 0;
            int processedFiles = 0;
            var stopwatch = Stopwatch.StartNew();
            byte[] buffer = new byte[DefaultBufferSize];

            foreach (var entry in archive.Entries)
            {
                cancellationToken.ThrowIfCancellationRequested();

                if (entry.IsDirectory)
                {
                    string dirPath = Path.GetFullPath(Path.Combine(destinationDirectory, entry.Key ?? ""));
                    EnsureDirectoryCreated(dirPath, createdDirs);
                    continue;
                }

                string key = entry.Key ?? Guid.NewGuid().ToString();
                string fullPath = Path.GetFullPath(Path.Combine(destinationDirectory, key));
                if (!fullPath.StartsWith(Path.GetFullPath(destinationDirectory), StringComparison.OrdinalIgnoreCase))
                    continue;

                string? parentDir = Path.GetDirectoryName(fullPath);
                if (!string.IsNullOrEmpty(parentDir))
                {
                    EnsureDirectoryCreated(parentDir, createdDirs);
                }

                using (var entryStream = entry.OpenEntryStream())
                using (var outStream = new FileStream(fullPath, FileMode.Create, FileAccess.Write, FileShare.None, DefaultBufferSize, FileOptions.SequentialScan))
                {
                    int read;
                    while ((read = entryStream.Read(buffer, 0, buffer.Length)) > 0)
                    {
                        cancellationToken.ThrowIfCancellationRequested();
                        outStream.Write(buffer, 0, read);

                        processedBytes += read;
                        ReportProgress(progress, Path.GetFileName(fullPath), processedBytes, totalBytes,
                            processedFiles, totalFiles, stopwatch);
                    }
                }

                processedFiles++;
                ReportProgress(progress, Path.GetFileName(fullPath), processedBytes, totalBytes,
                    processedFiles, totalFiles, stopwatch);
            }

            stopwatch.Stop();
            progress?.Report(new ExtractionProgressInfo
            {
                CurrentFile = "완료",
                ProcessedBytes = processedBytes,
                TotalBytes = totalBytes > 0 ? totalBytes : processedBytes,
                ProcessedFiles = processedFiles,
                TotalFiles = totalFiles > 0 ? totalFiles : processedFiles,
                Percentage = 100.0,
                SpeedBytesPerSec = processedBytes / Math.Max(0.001, stopwatch.Elapsed.TotalSeconds),
                RemainingTime = TimeSpan.Zero,
                StatusMessage = $"압축 풀기 완료 ({stopwatch.Elapsed.TotalSeconds:F2}초 소요)"
            });
        }

        /// <summary>
        /// 아카이브 파일의 기본 이름 반환 (.tar.gz, .7z.001 등 복합 확장자 안전 처리)
        /// </summary>
        public static string GetArchiveBaseName(string path)
        {
            string name = Path.GetFileName(path);
            string[] compoundExts = { ".tar.gz", ".tar.bz2", ".tar.xz", ".tar.zst", ".7z.001", ".zip.001" };
            foreach (var ext in compoundExts)
            {
                if (name.EndsWith(ext, StringComparison.OrdinalIgnoreCase))
                {
                    return name.Substring(0, name.Length - ext.Length);
                }
            }
            return Path.GetFileNameWithoutExtension(path);
        }

        /// <summary>
        /// 대상 폴더가 이미 존재하는 경우 "(2)", "(3)" 등으로 이름을 자동 증가시켜 충돌 방지
        /// </summary>
        public static string GetUniqueDirectoryPath(string parentDir, string baseFolderName)
        {
            string target = Path.Combine(parentDir, baseFolderName);
            if (!Directory.Exists(target) && !File.Exists(target))
            {
                return target;
            }

            int count = 2;
            while (true)
            {
                string candidate = Path.Combine(parentDir, $"{baseFolderName} ({count})");
                if (!Directory.Exists(candidate) && !File.Exists(candidate))
                {
                    return candidate;
                }
                count++;
            }
        }

        /// <summary>
        /// 대상 파일이 이미 존재하는 경우 "(2)", "(3)" 등으로 이름을 자동 증가시켜 충돌 방지
        /// </summary>
        public static string GetUniqueFilePath(string parentDir, string baseName, string extension)
        {
            if (!extension.StartsWith(".")) extension = "." + extension;
            string target = Path.Combine(parentDir, baseName + extension);
            if (!File.Exists(target) && !Directory.Exists(target))
            {
                return target;
            }

            int count = 2;
            while (true)
            {
                string candidate = Path.Combine(parentDir, $"{baseName} ({count}){extension}");
                if (!File.Exists(candidate) && !Directory.Exists(candidate))
                {
                    return candidate;
                }
                count++;
            }
        }

        /// <summary>
        /// 초고속 압축 생성 (ZIP)
        /// </summary>
        public static Task CompressAsync(
            IEnumerable<string> sourcePaths,
            string destinationArchive,
            System.IO.Compression.CompressionLevel compressionLevel = System.IO.Compression.CompressionLevel.Optimal,
            IProgress<ExtractionProgressInfo>? progress = null,
            CancellationToken cancellationToken = default)
        {
            return Task.Run(() =>
            {
                var filesToCompress = new List<(string FullPath, string RelativePath)>();
                long totalBytes = 0;

                string fullDestArchive = Path.GetFullPath(destinationArchive);

                foreach (var source in sourcePaths)
                {
                    if (string.IsNullOrWhiteSpace(source)) continue;
                    string fullSource = Path.GetFullPath(source);

                    if (File.Exists(fullSource))
                    {
                        // 자기 자신(출력 파일)은 제외
                        if (string.Equals(fullSource, fullDestArchive, StringComparison.OrdinalIgnoreCase))
                            continue;

                        var fi = new FileInfo(fullSource);
                        filesToCompress.Add((fullSource, Path.GetFileName(fullSource)));
                        totalBytes += fi.Length;
                    }
                    else if (Directory.Exists(fullSource))
                    {
                        string baseDir = Directory.GetParent(fullSource)?.FullName ?? fullSource;
                        var allFiles = Directory.GetFiles(fullSource, "*", SearchOption.AllDirectories);
                        foreach (var f in allFiles)
                        {
                            string fullF = Path.GetFullPath(f);
                            if (string.Equals(fullF, fullDestArchive, StringComparison.OrdinalIgnoreCase))
                                continue;

                            var fi = new FileInfo(fullF);
                            string rel = Path.GetRelativePath(baseDir, fullF).Replace('\\', '/');
                            filesToCompress.Add((fullF, rel));
                            totalBytes += fi.Length;
                        }
                    }
                }

                long processedBytes = 0;
                int processedFiles = 0;
                var stopwatch = Stopwatch.StartNew();

                string? destDir = Path.GetDirectoryName(fullDestArchive);
                if (!string.IsNullOrEmpty(destDir) && !Directory.Exists(destDir))
                {
                    Directory.CreateDirectory(destDir);
                }

                using (var zipStream = new FileStream(fullDestArchive, FileMode.Create, FileAccess.Write, FileShare.None, DefaultBufferSize))
                using (var archive = new System.IO.Compression.ZipArchive(zipStream, ZipArchiveMode.Create, false, EncodingHelper.KoreanEncoding))
                {
                    byte[] buffer = new byte[DefaultBufferSize];

                    foreach (var file in filesToCompress)
                    {
                        cancellationToken.ThrowIfCancellationRequested();

                        try
                        {
                            var entry = archive.CreateEntry(file.RelativePath, compressionLevel);
                            using (var src = new FileStream(file.FullPath, FileMode.Open, FileAccess.Read, FileShare.ReadWrite, DefaultBufferSize, FileOptions.SequentialScan))
                            using (var dst = entry.Open())
                            {
                                int read;
                                while ((read = src.Read(buffer, 0, buffer.Length)) > 0)
                                {
                                    cancellationToken.ThrowIfCancellationRequested();
                                    dst.Write(buffer, 0, read);

                                    processedBytes += read;
                                    ReportProgress(progress, Path.GetFileName(file.FullPath), processedBytes, totalBytes,
                                        processedFiles, filesToCompress.Count, stopwatch, "압축 중");
                                }
                            }
                        }
                        catch (OperationCanceledException)
                        {
                            throw;
                        }
                        catch (Exception ex)
                        {
                            Console.WriteLine($"[Compress warning] File {file.FullPath} skipped: {ex.Message}");
                        }

                        processedFiles++;
                        ReportProgress(progress, Path.GetFileName(file.FullPath), processedBytes, totalBytes,
                            processedFiles, filesToCompress.Count, stopwatch, "압축 중");
                    }
                }

                stopwatch.Stop();
                progress?.Report(new ExtractionProgressInfo
                {
                    CurrentFile = "완료",
                    ProcessedBytes = totalBytes,
                    TotalBytes = totalBytes,
                    ProcessedFiles = filesToCompress.Count,
                    TotalFiles = filesToCompress.Count,
                    Percentage = 100.0,
                    SpeedBytesPerSec = totalBytes / Math.Max(0.001, stopwatch.Elapsed.TotalSeconds),
                    RemainingTime = TimeSpan.Zero,
                    StatusMessage = $"압축 완료 ({stopwatch.Elapsed.TotalSeconds:F2}초 소요)"
                });
            }, cancellationToken);
        }

        /// <summary>
        /// 단일 파일 임시 폴더 추출 (더블클릭 실행용)
        /// </summary>
        public static Task<string> ExtractSingleFileAsync(string archivePath, string entryFullPath)
        {
            return Task.Run(() =>
            {
                string tempDir = Path.Combine(Path.GetTempPath(), "EasyZip_Temp", Path.GetRandomFileName());
                Directory.CreateDirectory(tempDir);
                string outPath = Path.Combine(tempDir, Path.GetFileName(entryFullPath));

                string ext = Path.GetExtension(archivePath).ToLowerInvariant();

                // 0. 분할 볼륨 (.001, .7z.001 등)
                if (MultiVolumeStream.IsSplitVolume(archivePath))
                {
                    using var mvs = new MultiVolumeStream(archivePath);
                    try
                    {
                        using var archive = ArchiveFactory.OpenArchive(mvs);
                        var entry = archive.Entries.FirstOrDefault(e => (e.Key ?? "").Replace('\\', '/') == entryFullPath.Replace('\\', '/'));
                        if (entry != null)
                        {
                            using var entryStream = entry.OpenEntryStream();
                            using var outStream = File.Create(outPath);
                            entryStream.CopyTo(outStream);
                            return outPath;
                        }
                    }
                    catch
                    {
                        mvs.Position = 0;
                        using var outStream = File.Create(outPath);
                        mvs.CopyTo(outStream);
                        return outPath;
                    }
                }

                if (ext == ".alz" || AlzReader.IsAlzFile(archivePath))
                {
                    return AlzReader.ExtractSingleFileAsync(archivePath, entryFullPath).GetAwaiter().GetResult();
                }

                if (ext == ".egg" || EggReader.IsEggFile(archivePath))
                {
                    return EggReader.ExtractSingleFileAsync(archivePath, entryFullPath).GetAwaiter().GetResult();
                }

                if (ext == ".zip")
                {
                    var enc = EncodingHelper.KoreanEncoding;
                    using var zip = System.IO.Compression.ZipFile.Open(archivePath, ZipArchiveMode.Read, enc);
                    var entry = zip.GetEntry(entryFullPath) 
                             ?? zip.Entries.FirstOrDefault(e => e.FullName.Replace('\\', '/') == entryFullPath.Replace('\\', '/'));

                    if (entry != null)
                    {
                        entry.ExtractToFile(outPath, true);
                        return outPath;
                    }
                }

                using (var archive = ArchiveFactory.OpenArchive(archivePath))
                {
                    var entry = archive.Entries.FirstOrDefault(e => (e.Key ?? "").Replace('\\', '/') == entryFullPath.Replace('\\', '/'));
                    if (entry != null)
                    {
                        using var entryStream = entry.OpenEntryStream();
                        using var outStream = File.Create(outPath);
                        entryStream.CopyTo(outStream);
                        return outPath;
                    }
                }

                throw new FileNotFoundException($"아카이브 내에서 '{entryFullPath}'를 찾을 수 없습니다.");
            });
        }

        private static void EnsureDirectoryCreated(string dirPath, ConcurrentDictionary<string, bool> cache)
        {
            if (string.IsNullOrEmpty(dirPath)) return;
            if (cache.TryAdd(dirPath, true))
            {
                if (!Directory.Exists(dirPath))
                {
                    Directory.CreateDirectory(dirPath);
                }
            }
        }

        private static void ReportProgress(
            IProgress<ExtractionProgressInfo>? progress,
            string currentFile,
            long processedBytes,
            long totalBytes,
            int processedFiles,
            int totalFiles,
            Stopwatch stopwatch,
            string action = "압축 해제 중")
        {
            if (progress == null) return;

            double elapsedSec = stopwatch.Elapsed.TotalSeconds;
            double speed = elapsedSec > 0.05 ? processedBytes / elapsedSec : 0;
            double pct = totalBytes > 0 ? Math.Min(100.0, (double)processedBytes / totalBytes * 100.0) : 0;

            TimeSpan? remaining = null;
            if (speed > 1024 && totalBytes > processedBytes)
            {
                double remainingSec = (totalBytes - processedBytes) / speed;
                if (remainingSec < 86400)
                {
                    remaining = TimeSpan.FromSeconds(remainingSec);
                }
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
                StatusMessage = $"{action}... ({processedFiles}/{totalFiles} 파일, {pct:F1}%)"
            });
        }

        /// <summary>
        /// 손상되거나 비정상적인 압축 파일에 대해 원인을 진단하여 친절한 안내 메시지 반환
        /// </summary>
        public static string DiagnoseArchiveError(string archivePath, Exception ex)
        {
            if (!File.Exists(archivePath))
                return "파일을 찾을 수 없습니다.";

            var fi = new FileInfo(archivePath);
            if (fi.Length == 0)
                return "0 바이트 크기의 빈 파일입니다. 정상적인 압축 데이터가 포함되어 있지 않습니다.";

            // 다운로드 관리자 임시 파일(.egpart, .crdownload, .part 등) 검사
            string dir = Path.GetDirectoryName(archivePath) ?? "";
            string fileName = Path.GetFileName(archivePath);

            string[] tempExtensions = { ".egpart", ".crdownload", ".part", ".downloading" };
            foreach (var tExt in tempExtensions)
            {
                string partFile1 = archivePath + tExt;
                string partFile2 = Path.Combine(dir, fileName + tExt);
                if (File.Exists(partFile1) || File.Exists(partFile2))
                {
                    return $"다운로드가 완료되지 않은 미완료 파일입니다.\n(다운로드 임시 파일 '{fileName + tExt}' 감지됨)\n다운로드가 100% 완료된 후 다시 열어주세요.";
                }
            }

            try
            {
                if (!string.IsNullOrEmpty(dir))
                {
                    var matches = Directory.GetFiles(dir, fileName + ".*")
                        .Where(f => tempExtensions.Any(ext => f.EndsWith(ext, StringComparison.OrdinalIgnoreCase)))
                        .ToList();
                    if (matches.Count > 0)
                    {
                        return $"다운로드가 완료되지 않은 미완료 파일입니다.\n(다운로드 임시 파일 '{Path.GetFileName(matches[0])}' 감지됨)\n다운로드가 100% 완료된 후 다시 열어주세요.";
                    }
                }
            }
            catch { }

            string fullEx = ex.ToString();
            if (fullEx.Contains("End of stream reached", StringComparison.OrdinalIgnoreCase) ||
                fullEx.Contains("Cannot seek to position", StringComparison.OrdinalIgnoreCase) ||
                fullEx.Contains("does not match header", StringComparison.OrdinalIgnoreCase) ||
                fullEx.Contains("파일 뒷부분이 잘린", StringComparison.OrdinalIgnoreCase))
            {
                string currentSizeStr = ArchiveItem.FormatBytes(fi.Length);
                return $"압축 파일이 다운로드 도중 중단되었거나 파일 뒷부분이 잘린 불완전한 파일입니다.\n\n" +
                       $"• 현재 디스크 파일 크기: {currentSizeStr}\n" +
                       $"• 원인 분석: 파일 크기가 원본보다 부족하여 압축 데이터 중간에 파일 끝(EOF)에 도달했습니다.\n" +
                       $"• 해결 방법: 원본 소스(웹 브라우저/토렌트 등)에서 다운로드가 100% 완료되었는지 확인하고 다시 다운로드해 주세요.\n\n" +
                       $"(세부 오류: {ex.Message})";
            }

            if (ex.Message.Contains("Zip Header", StringComparison.OrdinalIgnoreCase) ||
                ex.Message.Contains("Unknown Rar Header", StringComparison.OrdinalIgnoreCase))
            {
                return $"압축 파일의 헤더 정보가 손상되었거나 다운로드 도중 중단되어 잘린 파일입니다.\n\n세부 오류: {ex.Message}";
            }

            return ex.Message;
        }
    }
}
