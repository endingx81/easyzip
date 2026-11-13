using System;
using System.Runtime.InteropServices;

namespace EasyZip.Services
{
    public static class NativeMethods
    {
        [DllImport("shell32.dll", CharSet = CharSet.Auto, SetLastError = true)]
        public static extern void SHChangeNotify(
            uint wEventId,
            uint uFlags,
            IntPtr dwItem1,
            IntPtr dwItem2
        );

        public const uint SHCNE_ASSOCCHANGED = 0x08000000;
        public const uint SHCNF_IDLIST = 0x0000;
        public const uint SHCNF_FLUSH = 0x1000;

        /// <summary>
        /// 윈도우 탐색기에 파일 연결 및 쉘 컨텍스트 메뉴 변경 사항을 즉시 반영 알림
        /// </summary>
        public static void NotifyShellAssociationChanged()
        {
            try
            {
                SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, IntPtr.Zero, IntPtr.Zero);
            }
            catch
            {
                // Ignore
            }
        }
    }
}

