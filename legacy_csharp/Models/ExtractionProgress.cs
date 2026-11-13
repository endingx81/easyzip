using System;

namespace EasyZip.Models
{
    public class ExtractionProgressInfo
    {
        public string CurrentFile { get; set; } = string.Empty;
        public long ProcessedBytes { get; set; }
        public long TotalBytes { get; set; }
        public int ProcessedFiles { get; set; }
        public int TotalFiles { get; set; }
        public double Percentage { get; set; }
        public double SpeedBytesPerSec { get; set; }
        public TimeSpan? RemainingTime { get; set; }
        public string StatusMessage { get; set; } = string.Empty;

        public string FormattedSpeed
        {
            get
            {
                if (SpeedBytesPerSec < 1024)
                    return $"{SpeedBytesPerSec:F0} B/s";
                if (SpeedBytesPerSec < 1024 * 1024)
                    return $"{SpeedBytesPerSec / 1024.0:F1} KB/s";
                if (SpeedBytesPerSec < 1024 * 1024 * 1024)
                    return $"{SpeedBytesPerSec / (1024.0 * 1024.0):F1} MB/s";
                return $"{SpeedBytesPerSec / (1024.0 * 1024.0 * 1024.0):F2} GB/s";
            }
        }

        public string FormattedRemainingTime
        {
            get
            {
                if (!RemainingTime.HasValue || RemainingTime.Value.TotalSeconds <= 0)
                    return "계산 중...";
                var t = RemainingTime.Value;
                if (t.TotalHours >= 1)
                    return $"{(int)t.TotalHours}시간 {t.Minutes}분 {t.Seconds}초";
                if (t.TotalMinutes >= 1)
                    return $"{t.Minutes}분 {t.Seconds}초";
                return $"{t.Seconds}초";
            }
        }
    }
}

