using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.IO.Compression;
using System.Text;
using System.Threading;
using System.Threading.Tasks;
using EasyZip.Models;
using SharpCompress.Compressors.BZip2;

namespace EasyZip.Services
{
    public class AlzEntry
    {
        public string Name { get; set; } = string.Empty;
        public string FullPath { get; set; } = string.Empty;
        public bool IsDirectory { get; set; }
        public long UncompressedSize { get; set; }
        public long CompressedSize { get; set; }
        public DateTime? LastModified { get; set; }
        public uint Crc32 { get; set; }
        public byte CompressionMethod { get; set; } // 0: Stored, 1: Deflate, 2: BZip2
        public long DataOffset { get; set; }
    }

    public static class AlzReader
    {
        private const uint ALZ_SIGNATURE = 0x015A4C41; // ALZ\x01
        private const uint BLZ_FILE_HEADER = 0x015A4C42; // BLZ\x01
        private const uint CLZ_END_HEADER = 0x025A4C43; // CLZ\x02

        /// <summary>
        /// 주어진 파일이 ALZ 아카이브인지 확인
        /// </summary>
        public static bool IsAlzFile(string filePath)
        {
            try
            {
                using var fs = new FileStream(filePath, FileMode.Open, FileAccess.Read, FileShare.Read);
                if (fs.Length < 8) return false;
                using var br = new BinaryReader(fs);
                uint sig = br.ReadUInt32();
                return sig == ALZ_SIGNATURE;
            }
            catch
            {
                return false;
            }
        }

        /// <summary>
        /// ALZ 파일 내부의 엔트리 목록 파싱
        /// </summary>
        public static List<AlzEntry> ReadEntries(string filePath, Encoding? encoding = null)
        {
            var entries = new List<AlzEntry>();
            using var fs = new FileStream(filePath, FileMode.Open, FileAccess.Read, FileShare.Read);
            using var br = new BinaryReader(fs);

            if (fs.Length < 8) return entries;

            uint sig = br.ReadUInt32();
            if (sig != ALZ_SIGNATURE) return entries;

            // 헤더 추가 정보 (버전 등 4바이트 스킵)
            fs.Seek(8, SeekOrigin.Begin);

            while (fs.Position < fs.Length - 4)
            {
                uint headerSig = br.ReadUInt32();

                if (headerSig == CLZ_END_HEADER)
                {
                    break;
                }

                if (headerSig == BLZ_FILE_HEADER)
                {
                    ushort fileNameLen = br.ReadUInt16();
                    byte fileAttrib = br.ReadByte();
                    uint dosDateTime = br.ReadUInt32();
                    byte descriptorFlags = br.ReadByte();

                    // descriptorFlags 분석
                    int compSizeLen = (descriptorFlags & 0x0F);
                    int uncompSizeLen = ((descriptorFlags >> 4) & 0x0F);
                    if (compSizeLen == 0) compSizeLen = 4;
                    if (uncompSizeLen == 0) uncompSizeLen = 4;

                    byte compMethod = br.ReadByte();
                    uint crc = br.ReadUInt32();

                    long compSize = ReadVariableLengthNumber(br, compSizeLen);
                    long uncompSize = ReadVariableLengthNumber(br, uncompSizeLen);

                    byte[] fileNameBytes = br.ReadBytes(fileNameLen);
                    string rawPath = EncodingHelper.DecodeSmart(fileNameBytes, encoding);
                    string safePath = EncodingHelper.SanitizePath(rawPath);

                    bool isDir = (fileAttrib & 0x10) != 0 || safePath.EndsWith('/') || safePath.EndsWith('\\');
                    safePath = safePath.TrimEnd('/', '\\');

                    DateTime? modTime = DosDateTimeToDateTime(dosDateTime);

                    long dataOffset = fs.Position;

                    entries.Add(new AlzEntry
                    {
                        Name = Path.GetFileName(safePath),
                        FullPath = safePath,
                        IsDirectory = isDir,
                        CompressedSize = compSize,
                        UncompressedSize = uncompSize,
                        LastModified = modTime,
                        Crc32 = crc,
                        CompressionMethod = compMethod,
                        DataOffset = dataOffset
                    });

                    // 데이터 오프셋만큼 건너뛰기
                    if (compSize > 0)
                    {
                        fs.Seek(compSize, SeekOrigin.Current);
                    }
                }
                else
                {
                    // 알 수 없는 블록인 경우 1바이트씩 앞으로 진행하며 헤더 검색
                    fs.Seek(-3, SeekOrigin.Current);
                }
            }

            return entries;
        }

        /// <summary>
        /// ALZ 파일 전체 초고속 압축 해제
        /// </summary>
        public static async Task ExtractAllAsync(
            string alzPath,
            string destinationDirectory,
            IProgress<ExtractionProgressInfo>? progress = null,
            CancellationToken cancellationToken = default,
            Encoding? encoding = null)
        {
            await Task.Run(() =>
            {
                var entries = ReadEntries(alzPath, encoding);
                long totalBytes = 0;
                foreach (var e in entries)
                {
                    if (!e.IsDirectory) totalBytes += e.UncompressedSize;
                }

                long processedBytes = 0;
                int processedFiles = 0;
                var stopwatch = Stopwatch.StartNew();

                // 모든 대상 디렉터리 사전 일괄 생성
                foreach (var e in entries)
                {
                    string fullPath = Path.GetFullPath(Path.Combine(destinationDirectory, e.FullPath));
                    if (!fullPath.StartsWith(Path.GetFullPath(destinationDirectory), StringComparison.OrdinalIgnoreCase))
                        continue;

                    string? parentDir = e.IsDirectory ? fullPath : Path.GetDirectoryName(fullPath);
                    if (!string.IsNullOrEmpty(parentDir) && !Directory.Exists(parentDir))
                    {
                        Directory.CreateDirectory(parentDir);
                    }
                }

                using var fs = new FileStream(alzPath, FileMode.Open, FileAccess.Read, FileShare.Read, 1024 * 1024);

                foreach (var entry in entries)
                {
                    cancellationToken.ThrowIfCancellationRequested();

                    if (entry.IsDirectory)
                    {
                        continue;
                    }

                    string fullPath = Path.GetFullPath(Path.Combine(destinationDirectory, entry.FullPath));
                    if (!fullPath.StartsWith(Path.GetFullPath(destinationDirectory), StringComparison.OrdinalIgnoreCase))
                        continue;

                    fs.Seek(entry.DataOffset, SeekOrigin.Begin);

                    using var subStream = new SubStream(fs, entry.CompressedSize);
                    using var outStream = new FileStream(fullPath, FileMode.Create, FileAccess.Write, FileShare.None, 1024 * 1024);

                    Stream? decompressor = null;
                    try
                    {
                        if (entry.CompressionMethod == 0) // Stored
                        {
                            decompressor = subStream;
                        }
                        else if (entry.CompressionMethod == 1) // Deflate
                        {
                            decompressor = new DeflateStream(subStream, CompressionMode.Decompress, true);
                        }
                        else if (entry.CompressionMethod == 2) // BZip2
                        {
                            decompressor = BZip2Stream.Create(subStream, SharpCompress.Compressors.CompressionMode.Decompress, false, false, false);
                        }
                        else
                        {
                            // 기본 폴백 Deflate
                            decompressor = new DeflateStream(subStream, CompressionMode.Decompress, true);
                        }

                        byte[] buffer = new byte[1024 * 512];
                        int read;
                        long fileWritten = 0;

                        while ((read = decompressor.Read(buffer, 0, buffer.Length)) > 0)
                        {
                            cancellationToken.ThrowIfCancellationRequested();
                            outStream.Write(buffer, 0, read);
                            fileWritten += read;
                            processedBytes += read;

                            ReportProgress(progress, entry.Name, processedBytes, totalBytes, processedFiles, entries.Count, stopwatch);
                        }
                    }
                    catch (Exception)
                    {
                        // 일부 손상 또는 무시
                    }
                    finally
                    {
                        if (decompressor != null && decompressor != subStream)
                        {
                            decompressor.Dispose();
                        }
                    }

                    processedFiles++;
                    ReportProgress(progress, entry.Name, processedBytes, totalBytes, processedFiles, entries.Count, stopwatch);
                }

                stopwatch.Stop();
                progress?.Report(new ExtractionProgressInfo
                {
                    CurrentFile = "완료",
                    ProcessedBytes = totalBytes,
                    TotalBytes = totalBytes,
                    ProcessedFiles = processedFiles,
                    TotalFiles = entries.Count,
                    Percentage = 100.0,
                    SpeedBytesPerSec = totalBytes / Math.Max(0.001, stopwatch.Elapsed.TotalSeconds),
                    RemainingTime = TimeSpan.Zero,
                    StatusMessage = $"완료됨 ({stopwatch.Elapsed.TotalSeconds:F2}초 소요)"
                });
            }, cancellationToken);
        }

        /// <summary>
        /// ALZ 단일 파일 임시 추출 (더블클릭 실행용)
        /// </summary>
        public static async Task<string> ExtractSingleFileAsync(string alzPath, string entryFullPath, Encoding? encoding = null)
        {
            return await Task.Run(() =>
            {
                var entries = ReadEntries(alzPath, encoding);
                var entry = entries.Find(e => e.FullPath.Equals(entryFullPath, StringComparison.OrdinalIgnoreCase) ||
                                              e.Name.Equals(Path.GetFileName(entryFullPath), StringComparison.OrdinalIgnoreCase));

                if (entry == null) throw new FileNotFoundException($"ALZ 내부에서 '{entryFullPath}'를 찾을 수 없습니다.");

                string tempDir = Path.Combine(Path.GetTempPath(), "EasyZip_Temp", Path.GetRandomFileName());
                Directory.CreateDirectory(tempDir);
                string outPath = Path.Combine(tempDir, entry.Name);

                using var fs = new FileStream(alzPath, FileMode.Open, FileAccess.Read, FileShare.Read);
                fs.Seek(entry.DataOffset, SeekOrigin.Begin);

                using var subStream = new SubStream(fs, entry.CompressedSize);
                using var outStream = File.Create(outPath);

                Stream decompressor = entry.CompressionMethod switch
                {
                    0 => subStream,
                    1 => new DeflateStream(subStream, CompressionMode.Decompress, true),
                    2 => BZip2Stream.Create(subStream, SharpCompress.Compressors.CompressionMode.Decompress, false, false, false),
                    _ => new DeflateStream(subStream, CompressionMode.Decompress, true)
                };

                try
                {
                    decompressor.CopyTo(outStream);
                }
                finally
                {
                    if (decompressor != subStream) decompressor.Dispose();
                }

                return outPath;
            });
        }

        private static long ReadVariableLengthNumber(BinaryReader br, int length)
        {
            return length switch
            {
                1 => br.ReadByte(),
                2 => br.ReadUInt16(),
                4 => br.ReadUInt32(),
                8 => br.ReadInt64(),
                _ => br.ReadUInt32()
            };
        }

        private static DateTime? DosDateTimeToDateTime(uint dosDateTime)
        {
            try
            {
                ushort time = (ushort)(dosDateTime & 0xFFFF);
                ushort date = (ushort)((dosDateTime >> 16) & 0xFFFF);

                int year = ((date >> 9) & 0x7F) + 1980;
                int month = (date >> 5) & 0x0F;
                int day = date & 0x1F;

                int hour = (time >> 11) & 0x1F;
                int minute = (time >> 5) & 0x3F;
                int second = (time & 0x1F) * 2;

                if (year >= 1980 && month >= 1 && month <= 12 && day >= 1 && day <= 31)
                {
                    return new DateTime(year, month, day, Math.Min(23, hour), Math.Min(59, minute), Math.Min(59, second));
                }
            }
            catch { }
            return null;
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
                StatusMessage = $"ALZ 압축 해제 중... ({processedFiles}/{totalFiles} 파일, {pct:F1}%)"
            });
        }

        /// <summary>
        /// 원본 스트림의 특정 영역만 읽을 수 있도록 제한하는 서브 스트림
        /// </summary>
        private class SubStream : Stream
        {
            private readonly Stream _baseStream;
            private readonly long _length;
            private long _position;

            public SubStream(Stream baseStream, long length)
            {
                _baseStream = baseStream;
                _length = length;
                _position = 0;
            }

            public override bool CanRead => true;
            public override bool CanSeek => false;
            public override bool CanWrite => false;
            public override long Length => _length;
            public override long Position { get => _position; set => throw new NotSupportedException(); }

            public override int Read(byte[] buffer, int offset, int count)
            {
                if (_position >= _length) return 0;
                int toRead = (int)Math.Min(count, _length - _position);
                int read = _baseStream.Read(buffer, offset, toRead);
                _position += read;
                return read;
            }

            public override void Flush() { }
            public override long Seek(long offset, SeekOrigin origin) => throw new NotSupportedException();
            public override void SetLength(long value) => throw new NotSupportedException();
            public override void Write(byte[] buffer, int offset, int count) => throw new NotSupportedException();
        }
    }
}
