using System;
using System.Runtime.InteropServices;
using System.Windows;
using System.Windows.Interop;
using System.Windows.Controls;
using System.Windows.Media;

namespace Editor
{
    // P/Invoke declarations
    internal class NativeInterop
    {
        [DllImport("ApplicationDLL.dll", CallingConvention = CallingConvention.Cdecl)]
        public static extern IntPtr CreateNativeWindow();

        [DllImport("ApplicationDLL.dll", CallingConvention = CallingConvention.Cdecl)]
        public static extern void ShowNativeWindow();

        [DllImport("ApplicationDLL.dll", CallingConvention = CallingConvention.Cdecl)]
        public static extern void HideNativeWindow();

        [DllImport("ApplicationDLL.dll", CallingConvention = CallingConvention.Cdecl)]
        public static extern void DestroyNativeWindow();

        [DllImport("ApplicationDLL.dll", CallingConvention = CallingConvention.Cdecl)]
        public static extern void MessageLoopIteration();

        // ウィンドウ操作用のP/Invoke
        [DllImport("user32.dll")]
        public static extern bool SetParent(IntPtr hWndChild, IntPtr hWndNewParent);

        [DllImport("user32.dll")]
        public static extern bool MoveWindow(IntPtr hWnd, int x, int y, int cx, int cy, bool repaint);

        [DllImport("user32.dll")]
        public static extern bool SetWindowPos(IntPtr hWnd, IntPtr hWndInsertAfter, int x, int y, int cx, int cy, uint uFlags);

        private const uint SWP_NOZORDER = 0x0004;
    }

    public partial class MainWindow : Window
    {
        private IntPtr nativeHwnd = IntPtr.Zero;
        private System.Windows.Threading.DispatcherTimer messageLoopTimer;
        private HwndHost? hwndHost;

        public MainWindow()
        {
            InitializeComponent();
            this.Title = "WPF Native Window Manager";
            this.Width = 600;
            this.Height = 500;

            // メッセージループタイマーのセットアップ
            messageLoopTimer = new System.Windows.Threading.DispatcherTimer();
            messageLoopTimer.Interval = TimeSpan.FromMilliseconds(16); // ~60 FPS
            messageLoopTimer.Tick += MessageLoopTimer_Tick;
            messageLoopTimer.Start();
        }

        private void BtnCreate_Click(object sender, RoutedEventArgs e)
        {
            if (nativeHwnd == IntPtr.Zero)
            {
                nativeHwnd = NativeInterop.CreateNativeWindow();
                if (nativeHwnd != IntPtr.Zero)
                {
                    StatusText.Text = $"ウィンドウを作成しました (HWND: {nativeHwnd})";
                    BtnEmbed.IsEnabled = true;
                    BtnShow.IsEnabled = true;
                }
                else
                {
                    StatusText.Text = "ウィンドウの作成に失敗しました";
                }
            }
            else
            {
                StatusText.Text = "ウィンドウは既に作成済みです";
            }
        }

        private void BtnEmbed_Click(object sender, RoutedEventArgs e)
        {
            if (nativeHwnd != IntPtr.Zero && hwndHost == null)
            {
                try
                {
                    // HwndHostを作成してパネルに埋め込む
                    hwndHost = new NativeWindowHost(nativeHwnd);
                    NativeWindowPanel.Children.Clear();
                    NativeWindowPanel.Children.Add(hwndHost);

                    StatusText.Text = "ウィンドウをWPFに埋め込みました";
                    BtnEmbed.IsEnabled = false;
                    BtnShow.IsEnabled = false;
                }
                catch (Exception ex)
                {
                    StatusText.Text = $"埋め込みに失敗しました: {ex.Message}";
                }
            }
        }

        private void BtnShow_Click(object sender, RoutedEventArgs e)
        {
            if (nativeHwnd != IntPtr.Zero)
            {
                NativeInterop.ShowNativeWindow();
                StatusText.Text = "ウィンドウを表示しました";
            }
        }

        private void BtnHide_Click(object sender, RoutedEventArgs e)
        {
            if (nativeHwnd != IntPtr.Zero)
            {
                NativeInterop.HideNativeWindow();
                StatusText.Text = "ウィンドウを非表示にしました";
            }
        }

        private void BtnDestroy_Click(object sender, RoutedEventArgs e)
        {
            if (nativeHwnd != IntPtr.Zero)
            {
                NativeInterop.DestroyNativeWindow();
                hwndHost?.Dispose();
                hwndHost = null;
                NativeWindowPanel.Children.Clear();
                nativeHwnd = IntPtr.Zero;

                StatusText.Text = "ウィンドウを破棄しました";
                BtnEmbed.IsEnabled = false;
                BtnShow.IsEnabled = false;
            }
        }

        /// <summary>
        /// メインループのタイマーイベント
        /// </summary>
        /// <param name="sender"></param>
        /// <param name="e"></param>
        private void MessageLoopTimer_Tick(object? sender, EventArgs e)
        {
            if (nativeHwnd != IntPtr.Zero)
            {
                NativeInterop.MessageLoopIteration();
            }
        }

        protected override void OnClosed(EventArgs e)
        {
            messageLoopTimer?.Stop();

            if (nativeHwnd != IntPtr.Zero)
            {
                NativeInterop.DestroyNativeWindow();
            }

            hwndHost?.Dispose();
            base.OnClosed(e);
        }
    }

    // ネイティブウィンドウをWPFに埋め込むためのHwndHost
    public class NativeWindowHost : HwndHost
    {
        private IntPtr childHandle;
        private const int CW_USEDEFAULT = unchecked((int)0x80000000);

        public NativeWindowHost(IntPtr hwnd)
        {
            childHandle = hwnd;
        }

        protected override HandleRef BuildWindowCore(HandleRef hwndParent)
        {
            // ネイティブウィンドウの親をWPFウィンドウに設定
            NativeInterop.SetParent(childHandle, hwndParent.Handle);

            // ウィンドウスタイルを調整（ボーダーなし等）
            SetWindowLong(childHandle, GWL_STYLE,
                (GetWindowLong(childHandle, GWL_STYLE) & ~WS_POPUP) | WS_CHILD);

            // 表示
            NativeInterop.ShowNativeWindow();

            return new HandleRef(this, childHandle);
        }

        protected override void DestroyWindowCore(HandleRef hwnd)
        {
            // 親との関係を解除
            NativeInterop.SetParent(hwnd.Handle, IntPtr.Zero);
        }

        protected override void OnWindowPositionChanged(Rect rcBounds)
        {
            // ネイティブウィンドウのサイズと位置をWPFに合わせる
            NativeInterop.MoveWindow(
                childHandle,
                (int)rcBounds.X,
                (int)rcBounds.Y,
                (int)rcBounds.Width,
                (int)rcBounds.Height,
                true);
        }

        [DllImport("user32.dll", SetLastError = true)]
        private static extern int GetWindowLong(IntPtr hWnd, int nIndex);

        [DllImport("user32.dll", SetLastError = true)]
        private static extern int SetWindowLong(IntPtr hWnd, int nIndex, int dwNewLong);

        private const int GWL_STYLE = -16;
        private const int WS_CHILD = 0x40000000;
        private const int WS_POPUP = unchecked((int)0x80000000);
    }
}