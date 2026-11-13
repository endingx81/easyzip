using System;
using System.IO;
using System.Text;

namespace EasyZip.Services
{
    public static class EncodingHelper
    {
        private static bool _initialized = false;

        public static Encoding KoreanEncoding { get; private set; } = Encoding.UTF8;
        public static Encoding JapaneseEncoding { get; private set; } = Encoding.UTF8;
        public static Encoding ChineseEncoding { get; private set; } = Encoding.UTF8;

        public static void Initialize()
        {
            if (_initialized) return;

            try
            {
                // .NET 8에서 CP949, Shift-JIS, GB2312, EUC-KR 등 모든 코드페이지 활성화
                Encoding.RegisterProvider(CodePagesEncodingProvider.Instance);

                KoreanEncoding = Encoding.GetEncoding(949); // CP949 / EUC-KR
                JapaneseEncoding = Encoding.GetEncoding(932); // Shift-JIS
                ChineseEncoding = Encoding.GetEncoding(936); // GBK / GB2312
            }
            catch
            {
                KoreanEncoding = Encoding.UTF8;
                JapaneseEncoding = Encoding.UTF8;
                ChineseEncoding = Encoding.UTF8;
            }

            _initialized = true;
        }

        /// <summary>
        /// 바이트 배열의 인코딩을 지능적으로 감지 (UTF-8, 한국어 CP949, 일본어 Shift-JIS, 시스템 기본)
        /// 다국어 아카이브에서 언어 깨짐으로 압축 해제가 실패하는 문제를 원천 방지
        /// </summary>
        public static string DecodeSmart(byte[] rawBytes, Encoding? preferredEncoding = null)
        {
            if (rawBytes == null || rawBytes.Length == 0) return string.Empty;

            Initialize();

            if (preferredEncoding != null)
            {
                try
                {
                    return preferredEncoding.GetString(rawBytes);
                }
                catch { }
            }

            // 1. 유효한 UTF-8인지 검사
            if (IsValidUtf8(rawBytes))
            {
                string text = Encoding.UTF8.GetString(rawBytes);
                // 제어 문자 등이 없고 정상 문자열이면 채택
                if (!HasControlChars(text))
                {
                    return text;
                }
            }

            // 2. 한국어 Windows에서 가장 빈번한 CP949 시도
            try
            {
                string kor = KoreanEncoding.GetString(rawBytes);
                if (!kor.Contains('\uFFFD') && !HasIllegalPathChars(kor))
                {
                    return kor;
                }
            }
            catch { }

            // 3. 일본어 Shift-JIS 시도
            try
            {
                string jpn = JapaneseEncoding.GetString(rawBytes);
                if (!jpn.Contains('\uFFFD') && !HasIllegalPathChars(jpn))
                {
                    return jpn;
                }
            }
            catch { }

            // 4. 시스템 기본 ANSI 인코딩
            try
            {
                return Encoding.Default.GetString(rawBytes);
            }
            catch
            {
                return Encoding.UTF8.GetString(rawBytes);
            }
        }

        /// <summary>
        /// Windows 파일 시스템에서 허용되지 않는 문자들을 안전하게 정제
        /// 파일명 깨짐으로 인한 DirectoryNotFoundException / ArgumentException 방지
        /// </summary>
        public static string SanitizePath(string path)
        {
            if (string.IsNullOrEmpty(path)) return "unnamed_item";

            // 정규화: 역슬래시를 슬래시로 통일
            path = path.Replace('\\', '/').Trim();

            // 상위 디렉터리 탈출(Zip Slip 취약점) 차단
            path = path.Replace("../", "").Replace("..\\", "");

            string[] parts = path.Split('/');
            char[] invalidChars = Path.GetInvalidFileNameChars();

            for (int i = 0; i < parts.Length; i++)
            {
                string part = parts[i];
                if (string.IsNullOrWhiteSpace(part)) continue;

                var sb = new StringBuilder(part.Length);
                foreach (char c in part)
                {
                    // 제어 문자 및 파일명 금지 문자 치환
                    if (c < 32 || Array.IndexOf(invalidChars, c) >= 0)
                    {
                        sb.Append('_');
                    }
                    else
                    {
                        sb.Append(c);
                    }
                }

                string sanitized = sb.ToString().Trim('.', ' ');
                // Windows 예약어 (CON, PRN, AUX, NUL, COM1..9, LPT1..9) 처리
                if (IsReservedName(sanitized))
                {
                    sanitized = "_" + sanitized;
                }

                parts[i] = string.IsNullOrEmpty(sanitized) ? "item" : sanitized;
            }

            return string.Join('/', parts);
        }

        private static bool IsValidUtf8(byte[] bytes)
        {
            int i = 0;
            int len = bytes.Length;
            bool hasMultiByte = false;

            while (i < len)
            {
                byte b = bytes[i];
                if ((b & 0x80) == 0) // 1바이트 ASCII
                {
                    i++;
                }
                else if ((b & 0xE0) == 0xC0) // 2바이트
                {
                    if (i + 1 >= len || (bytes[i + 1] & 0xC0) != 0x80) return false;
                    hasMultiByte = true;
                    i += 2;
                }
                else if ((b & 0xF0) == 0xE0) // 3바이트 (한글 등)
                {
                    if (i + 2 >= len || (bytes[i + 1] & 0xC0) != 0x80 || (bytes[i + 2] & 0xC0) != 0x80) return false;
                    hasMultiByte = true;
                    i += 3;
                }
                else if ((b & 0xF8) == 0xF0) // 4바이트
                {
                    if (i + 3 >= len || (bytes[i + 1] & 0xC0) != 0x80 || (bytes[i + 2] & 0xC0) != 0x80 || (bytes[i + 3] & 0xC0) != 0x80) return false;
                    hasMultiByte = true;
                    i += 4;
                }
                else
                {
                    return false;
                }
            }

            return hasMultiByte; // 멀티바이트가 전혀 없는 순수 ASCII는 UTF-8이자 ANSI이므로 true
        }

        private static bool HasControlChars(string s)
        {
            foreach (char c in s)
            {
                if (c < 32 && c != '\t' && c != '\r' && c != '\n') return true;
            }
            return false;
        }

        private static bool HasIllegalPathChars(string s)
        {
            char[] invalid = Path.GetInvalidPathChars();
            return s.IndexOfAny(invalid) >= 0;
        }

        private static bool IsReservedName(string name)
        {
            string upper = name.ToUpperInvariant();
            return upper is "CON" or "PRN" or "AUX" or "NUL"
                or "COM1" or "COM2" or "COM3" or "COM4" or "COM5" or "COM6" or "COM7" or "COM8" or "COM9"
                or "LPT1" or "LPT2" or "LPT3" or "LPT4" or "LPT5" or "LPT6" or "LPT7" or "LPT8" or "LPT9";
        }
    }
}
