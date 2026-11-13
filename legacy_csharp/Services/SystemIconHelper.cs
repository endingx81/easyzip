using System;
using System.Collections.Concurrent;
using System.IO;
using System.Runtime.InteropServices;
using System.Windows;
using System.Windows.Interop;
using System.Windows.Media;
using System.Windows.Media.Imaging;

namespace EasyZip.Services
{
    public static class SystemIconHelper
    {
        [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Unicode)]
        private struct SHFILEINFO
        {
            public IntPtr hIcon;
            public int iIcon;
            public uint dwAttributes;
            [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 260)]
            public string szDisplayName;
            [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 80)]
            public string szTypeName;
        }

        [DllImport("shell32.dll", CharSet = CharSet.Unicode)]
        private static extern IntPtr SHGetFileInfo(
            string pszPath,
            uint dwFileAttributes,
            ref SHFILEINFO psfi,
            uint cbFileInfo,
            uint uFlags);

        [DllImport("user32.dll", SetLastError = true)]
        [return: MarshalAs(UnmanagedType.Bool)]
        private static extern bool DestroyIcon(IntPtr hIcon);

        private const uint SHGFI_ICON = 0x000000100;
        private const uint SHGFI_SMALLICON = 0x000000001;
        private const uint SHGFI_USEFILEATTRIBUTES = 0x000000010;

        private const uint FILE_ATTRIBUTE_NORMAL = 0x00000080;
        private const uint FILE_ATTRIBUTE_DIRECTORY = 0x00000010;

        private static readonly ConcurrentDictionary<string, ImageSource> _iconCache = new(StringComparer.OrdinalIgnoreCase);
        private static ImageSource? _folderIcon;

        /// <summary>
        /// 윈도우 탐색기 표준 폴더 아이콘 반환
        /// </summary>
        public static ImageSource GetFolderIcon()
        {
            if (_folderIcon != null) return _folderIcon;

            _folderIcon = GetIconByAttributes("folder", FILE_ATTRIBUTE_DIRECTORY);
            return _folderIcon;
        }

        /// <summary>
        /// 윈도우 탐색기와 100% 동일한 확장자별 시스템 아이콘 반환
        /// </summary>
        public static ImageSource GetFileIcon(string filename)
        {
            if (string.IsNullOrEmpty(filename)) return GetDefaultFileIcon();

            string ext = Path.GetExtension(filename).ToLowerInvariant();
            if (string.IsNullOrEmpty(ext)) ext = ".file";

            return _iconCache.GetOrAdd(ext, extension =>
            {
                return GetIconByAttributes("dummy" + extension, FILE_ATTRIBUTE_NORMAL);
            });
        }

        private static ImageSource GetDefaultFileIcon()
        {
            return _iconCache.GetOrAdd(".default", _ => GetIconByAttributes("dummy.txt", FILE_ATTRIBUTE_NORMAL));
        }

        private static ImageSource GetIconByAttributes(string path, uint attributes)
        {
            try
            {
                var shinfo = new SHFILEINFO();
                IntPtr res = SHGetFileInfo(
                    path,
                    attributes,
                    ref shinfo,
                    (uint)Marshal.SizeOf(shinfo),
                    SHGFI_ICON | SHGFI_SMALLICON | SHGFI_USEFILEATTRIBUTES);

                if (res != IntPtr.Zero && shinfo.hIcon != IntPtr.Zero)
                {
                    try
                    {
                        var bs = Imaging.CreateBitmapSourceFromHIcon(
                            shinfo.hIcon,
                            Int32Rect.Empty,
                            BitmapSizeOptions.FromEmptyOptions());
                        bs.Freeze();
                        return bs;
                    }
                    finally
                    {
                        DestroyIcon(shinfo.hIcon);
                    }
                }
            }
            catch
            {
                // Fallback
            }

            return CreateFallbackBitmap();
        }

        private static ImageSource CreateFallbackBitmap()
        {
            var drawingVisual = new DrawingVisual();
            using (var dc = drawingVisual.RenderOpen())
            {
                dc.DrawRectangle(Brushes.LightGray, null, new Rect(0, 0, 16, 16));
            }
            var rtb = new RenderTargetBitmap(16, 16, 96, 96, PixelFormats.Pbgra32);
            rtb.Render(drawingVisual);
            rtb.Freeze();
            return rtb;
        }
    }
}

