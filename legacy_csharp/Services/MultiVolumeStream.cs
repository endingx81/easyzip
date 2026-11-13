using System;
using System.Collections.Generic;
using System.IO;
using System.Text.RegularExpressions;

namespace EasyZip.Services
{
    /// <summary>
    /// .001, .002, .003 등 분할 압축 볼륨 파일들을 하나의 연속적이고 탐색(Seek) 가능한 스트림으로 가상화
    /// 7-Zip 분할 압축(.7z.001), 분할 ZIP(.zip.001), 원시 분할 파일(.001) 완벽 지원
    /// </summary>
    public class MultiVolumeStream : Stream
    {
        private readonly List<string> _partPaths = new();
        private readonly List<long> _partLengths = new();
        private readonly List<long> _partOffsets = new();
        private readonly long _totalLength;

        private long _position = 0;
        private int _currentPartIndex = -1;
        private FileStream? _currentStream;
        private bool _disposed;

        public IReadOnlyList<string> PartPaths => _partPaths;

        public MultiVolumeStream(string firstOrAnyPartPath)
        {
            var parts = FindAllParts(firstOrAnyPartPath);
            if (parts.Count == 0)
                throw new FileNotFoundException("분할 볼륨 파일을 찾을 수 없습니다.", firstOrAnyPartPath);

            _partPaths = parts;

            long runningOffset = 0;
            foreach (var path in _partPaths)
            {
                var fi = new FileInfo(path);
                long len = fi.Length;
                _partLengths.Add(len);
                _partOffsets.Add(runningOffset);
                runningOffset += len;
            }

            _totalLength = runningOffset;
        }

        /// <summary>
        /// .001 또는 .002 등 임의의 분할 파트 경로가 주어졌을 때 전체 파트 목록(.001, .002, ...)을 순서대로 검색
        /// </summary>
        public static List<string> FindAllParts(string filePath)
        {
            var result = new List<string>();
            if (!File.Exists(filePath)) return result;

            string? dir = Path.GetDirectoryName(filePath);
            if (string.IsNullOrEmpty(dir)) dir = ".";

            string fileName = Path.GetFileName(filePath);

            // 파일명이 .001, .002 등 숫자로 끝나는 패턴 검사 (예: test.7z.001 또는 test.001)
            var match = Regex.Match(fileName, @"^(.*)\.(\d{3,})$", RegexOptions.IgnoreCase);
            if (!match.Success)
            {
                // 숫자로 끝나는 분할 확장자가 아닌 경우 단일 파일로 반환
                result.Add(filePath);
                return result;
            }

            string basePrefix = match.Groups[1].Value;
            int numDigits = match.Groups[2].Value.Length;

            // 001부터 순차적으로 존재하는 모든 파트 탐색
            int index = 1;
            while (true)
            {
                string partExt = index.ToString().PadLeft(numDigits, '0');
                string expectedFileName = $"{basePrefix}.{partExt}";
                string expectedPath = Path.Combine(dir, expectedFileName);

                if (File.Exists(expectedPath))
                {
                    result.Add(expectedPath);
                    index++;
                }
                else
                {
                    break;
                }
            }

            // 만약 001부터 시작하지 않는 경우 (예: 000부터 시작하는 분할 도구)
            if (result.Count == 0)
            {
                string zeroPart = $"{basePrefix}.{"0".PadLeft(numDigits, '0')}";
                string zeroPath = Path.Combine(dir, zeroPart);
                if (File.Exists(zeroPath))
                {
                    index = 0;
                    while (true)
                    {
                        string partExt = index.ToString().PadLeft(numDigits, '0');
                        string p = Path.Combine(dir, $"{basePrefix}.{partExt}");
                        if (File.Exists(p))
                        {
                            result.Add(p);
                            index++;
                        }
                        else break;
                    }
                }
            }

            if (result.Count == 0)
            {
                result.Add(filePath);
            }

            return result;
        }

        public static bool IsSplitVolume(string filePath)
        {
            if (string.IsNullOrEmpty(filePath)) return false;
            string ext = Path.GetExtension(filePath);
            return Regex.IsMatch(ext, @"^\.\d{3,}$", RegexOptions.IgnoreCase);
        }

        public override bool CanRead => true;
        public override bool CanSeek => true;
        public override bool CanWrite => false;
        public override long Length => _totalLength;

        public override long Position
        {
            get => _position;
            set => Seek(value, SeekOrigin.Begin);
        }

        public override int Read(byte[] buffer, int offset, int count)
        {
            if (_disposed) throw new ObjectDisposedException(nameof(MultiVolumeStream));
            if (count == 0 || _position >= _totalLength) return 0;

            int partIndex = GetPartIndexForPosition(_position);
            if (partIndex < 0 || partIndex >= _partPaths.Count) return 0;

            EnsurePartStream(partIndex);

            long partStart = _partOffsets[partIndex];
            long localOffset = _position - partStart;
            long partRemaining = _partLengths[partIndex] - localOffset;

            int toRead = (int)Math.Min(count, partRemaining);

            if (_currentStream!.Position != localOffset)
            {
                _currentStream.Seek(localOffset, SeekOrigin.Begin);
            }

            int bytesRead = _currentStream.Read(buffer, offset, toRead);
            _position += bytesRead;

            return bytesRead;
        }

        public override long Seek(long offset, SeekOrigin origin)
        {
            if (_disposed) throw new ObjectDisposedException(nameof(MultiVolumeStream));

            long target = origin switch
            {
                SeekOrigin.Begin => offset,
                SeekOrigin.Current => _position + offset,
                SeekOrigin.End => _totalLength + offset,
                _ => throw new ArgumentOutOfRangeException(nameof(origin))
            };

            if (target < 0) throw new IOException("스트림 시작 위치보다 앞으로 이동할 수 없습니다.");
            if (target > _totalLength) target = _totalLength;

            _position = target;
            return _position;
        }

        private int GetPartIndexForPosition(long pos)
        {
            if (pos >= _totalLength) return _partPaths.Count - 1;

            for (int i = 0; i < _partOffsets.Count; i++)
            {
                long start = _partOffsets[i];
                long end = start + _partLengths[i];
                if (pos >= start && pos < end)
                {
                    return i;
                }
            }

            return _partPaths.Count - 1;
        }

        private void EnsurePartStream(int partIndex)
        {
            if (_currentPartIndex == partIndex && _currentStream != null)
                return;

            _currentStream?.Dispose();
            _currentStream = new FileStream(_partPaths[partIndex], FileMode.Open, FileAccess.Read, FileShare.Read, 1024 * 1024, FileOptions.SequentialScan);
            _currentPartIndex = partIndex;
        }

        public override void Flush() { }

        public override void SetLength(long value) => throw new NotSupportedException();

        public override void Write(byte[] buffer, int offset, int count) => throw new NotSupportedException();

        protected override void Dispose(bool disposing)
        {
            if (!_disposed)
            {
                if (disposing)
                {
                    _currentStream?.Dispose();
                    _currentStream = null;
                }
                _disposed = true;
            }
            base.Dispose(disposing);
        }
    }
}

