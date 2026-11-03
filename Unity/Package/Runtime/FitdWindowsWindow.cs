#if UNITY_STANDALONE_WIN && !UNITY_EDITOR
using System;
using System.Runtime.InteropServices;
using System.Text;

namespace Fitd.Unity
{
    internal static class FitdWindowsWindow
    {
        [StructLayout(LayoutKind.Sequential)]
        private struct Rect { public int left, top, right, bottom; }

        private delegate bool EnumerateWindow(IntPtr window, IntPtr parameter);

        [DllImport("user32.dll")] private static extern bool EnumWindows(EnumerateWindow callback, IntPtr parameter);
        [DllImport("user32.dll")] private static extern uint GetWindowThreadProcessId(IntPtr window, out uint process);
        [DllImport("kernel32.dll")] private static extern uint GetCurrentProcessId();
        [DllImport("user32.dll", CharSet = CharSet.Unicode)] private static extern int GetClassName(IntPtr window, StringBuilder name, int capacity);
        [DllImport("user32.dll")] private static extern bool GetClientRect(IntPtr window, out Rect rectangle);
        [DllImport("user32.dll")] private static extern bool GetWindowRect(IntPtr window, out Rect rectangle);
        [DllImport("user32.dll")] private static extern bool SetWindowPos(IntPtr window, IntPtr after,
            int x, int y, int width, int height, uint flags);

        public static void ResizeClient(int width, int height)
        {
            uint process = GetCurrentProcessId();
            IntPtr playerWindow = IntPtr.Zero;
            var name = new StringBuilder(64);
            EnumWindows((window, _) =>
            {
                GetWindowThreadProcessId(window, out uint owner);
                if (owner != process || GetClassName(window, name, name.Capacity) == 0 ||
                    name.ToString() != "UnityWndClass") return true;
                playerWindow = window;
                return false;
            }, IntPtr.Zero);
            if (playerWindow == IntPtr.Zero || !GetClientRect(playerWindow, out var client) ||
                !GetWindowRect(playerWindow, out var frame)) return;
            int horizontalBorder = frame.right - frame.left - (client.right - client.left);
            int verticalBorder = frame.bottom - frame.top - (client.bottom - client.top);
            // Keep activation, position, and z-order under Unity's control.
            const uint noMove = 0x0002, noZOrder = 0x0004, noActivate = 0x0010;
            SetWindowPos(playerWindow, IntPtr.Zero, 0, 0, width + horizontalBorder,
                height + verticalBorder, noMove | noZOrder | noActivate);
        }
    }
}
#endif
