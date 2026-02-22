using System;
using System.Diagnostics;
using System.IO;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Interop;
using System.Windows.Media;

namespace Editor
{

    internal static class NativeLoaderDiagnostics
    {
        private const uint LOAD_WITH_ALTERED_SEARCH_PATH = 0x00000008;
        private const uint FORMAT_MESSAGE_FROM_SYSTEM = 0x00001000;
        private const uint FORMAT_MESSAGE_IGNORE_INSERTS = 0x00000200;

        [DllImport("kernel32", SetLastError = true, CharSet = CharSet.Unicode)]
        private static extern IntPtr LoadLibraryEx(string lpFileName, IntPtr hFile, uint dwFlags);

        [DllImport("kernel32", SetLastError = true)]
        private static extern bool FreeLibrary(IntPtr hModule);

        [DllImport("kernel32", SetLastError = true, CharSet = CharSet.Unicode)]
        private static extern uint FormatMessage(uint dwFlags, IntPtr lpSource, uint dwMessageId,
            uint dwLanguageId, [Out] StringBuilder lpBuffer, uint nSize, IntPtr Arguments);

        public static bool TryPreloadDll(string dllFileName, out string message)
        {
            message = string.Empty;
            try
            {
                string baseDir = AppContext.BaseDirectory ?? Environment.CurrentDirectory;
                string fullPath = Path.IsPathRooted(dllFileName) ? dllFileName : Path.Combine(baseDir, dllFileName);

                if (!File.Exists(fullPath))
                {
                    message = $"DLL が見つかりません: {fullPath}";
                    return false;
                }

                IntPtr h = LoadLibraryEx(fullPath, IntPtr.Zero, LOAD_WITH_ALTERED_SEARCH_PATH);
                if (h == IntPtr.Zero)
                {
                    int err = Marshal.GetLastWin32Error();
                    message = $"LoadLibraryEx に失敗しました (GetLastError=0x{err:X8}): {GetSystemMessage((uint)err)}";
                    return false;
                }

                // 成功: ただちに解放（ただの診断）
                FreeLibrary(h);
                message = "DLL のロードに成功しました（依存関係も満たされている可能性が高い）";
                return true;
            }
            catch (Exception ex)
            {
                message = $"診断中に例外: {ex.GetType().Name} - {ex.Message}";
                return false;
            }
        }

        private static string GetSystemMessage(uint code)
        {
            var sb = new StringBuilder(512);
            uint flags = FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS;
            uint len = FormatMessage(flags, IntPtr.Zero, code, 0, sb, (uint)sb.Capacity, IntPtr.Zero);
            if (len == 0) return $"システムメッセージが取得できません (コード={code})";
            return sb.ToString().Trim();
        }
    }
    // P/Invoke declarations
    internal class NativeInterop
    {
        [DllImport("ApplicationDLL.dll", CallingConvention = CallingConvention.Cdecl, SetLastError = true)]
        public static extern IntPtr CreateNativeWindow();

        [DllImport("ApplicationDLL.dll", CallingConvention = CallingConvention.Cdecl, SetLastError = true)]
        public static extern void ShowNativeWindow();

        [DllImport("ApplicationDLL.dll", CallingConvention = CallingConvention.Cdecl, SetLastError = true)]
        public static extern void HideNativeWindow();

        [DllImport("ApplicationDLL.dll", CallingConvention = CallingConvention.Cdecl, SetLastError = true)]
        public static extern void DestroyNativeWindow();

        [DllImport("ApplicationDLL.dll", CallingConvention = CallingConvention.Cdecl, SetLastError = true)]
        public static extern void MessageLoopIteration();

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        public delegate void PieTickCallback(float deltaSeconds);

        [DllImport("ApplicationDLL.dll", CallingConvention = CallingConvention.Cdecl, SetLastError = true)]
        public static extern void SetPieTickCallback(PieTickCallback? callback);

        [DllImport("ApplicationDLL.dll", CallingConvention = CallingConvention.Cdecl, SetLastError = true)]
        [return: MarshalAs(UnmanagedType.Bool)]
        public static extern bool IsPieRunning();

        // ウィンドウ操作用のP/Invoke
        [DllImport("user32.dll", SetLastError = true)]
        public static extern bool SetParent(IntPtr hWndChild, IntPtr hWndNewParent);

        [DllImport("user32.dll", SetLastError = true)]
        public static extern bool MoveWindow(IntPtr hWnd, int x, int y, int cx, int cy, bool repaint);

        [DllImport("user32.dll", SetLastError = true)]
        public static extern bool SetWindowPos(IntPtr hWnd, IntPtr hWndInsertAfter, int x, int y, int cx, int cy, uint uFlags);

        private const uint SWP_NOZORDER = 0x0004;
    }

    public partial class MainWindow : Window
    {
        private IntPtr nativeHwnd = IntPtr.Zero;
        private System.Windows.Threading.DispatcherTimer messageLoopTimer;
        private HwndHost? hwndHost;
        private readonly PieGameHost pieGameHost = new();
        private NativeInterop.PieTickCallback? pieTickCallback;
        private bool wasPieRunning = false;

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
                // 事前チェックと安全呼び出し
                string reason;
                IntPtr hwnd = SafeCreateNativeWindow(out reason);
                if (hwnd != IntPtr.Zero)
                {
                    nativeHwnd = hwnd;
                    pieTickCallback = OnPieTickFromNative;
                    NativeInterop.SetPieTickCallback(pieTickCallback);
                    StatusText.Text = $"ウィンドウを作成しました (HWND: {nativeHwnd})";
                    BtnEmbed.IsEnabled = true;
                    BtnShow.IsEnabled = true;
                }
                else
                {
                    // 失敗理由を表示（詳細はログにも残す）
                    StatusText.Text = $"ウィンドウの作成に失敗しました: {reason}";
                    Debug.WriteLine($"CreateNativeWindow failed: {reason}");
                }
            }
            else
            {
                StatusText.Text = "ウィンドウは既に作成済みです";
            }
        }

        // CreateNativeWindow を呼ぶ前に DLL の存在・アーキテクチャを確認し、例外を安全に捕捉する
        private IntPtr SafeCreateNativeWindow(out string reason)
        {
            reason = string.Empty;

            try
            {
                string baseDir = AppContext.BaseDirectory ?? Environment.CurrentDirectory;
                string dllPath = Path.Combine(baseDir, "ApplicationDLL.dll");

                if (!File.Exists(dllPath))
                {
                    reason = $"DLL が見つかりません: {dllPath}";
                    return IntPtr.Zero;
                }

                // プロセスのビット幅と DLL の PE ヘッダを比較して不整合を検出
                bool dllIs64 = PeUtils.IsDll64Bit(dllPath);
                if (Environment.Is64BitProcess && !dllIs64)
                {
                    reason = "プロセスは64ビットですが、DLL は32ビットです（アーキテクチャ不一致）";
                    return IntPtr.Zero;
                }
                if (!Environment.Is64BitProcess && dllIs64)
                {
                    reason = "プロセスは32ビットですが、DLL は64ビットです（アーキテクチャ不一致）";
                    return IntPtr.Zero;
                }

                try
                {
                    // 実際の呼び出しは例外保護する
                    IntPtr hwnd = NativeInterop.CreateNativeWindow();
                    if (hwnd == IntPtr.Zero)
                    {
                        // ネイティブ側で失敗して NULL を返したケース
                        int err = Marshal.GetLastWin32Error();
                        reason = $"ネイティブ関数が NULL を返しました。GetLastError={err}";
                        return IntPtr.Zero;
                    }
                    return hwnd;
                }
                catch (DllNotFoundException ex)
                {
                    reason = $"DLL ロード失敗: {ex.Message}";
                    return IntPtr.Zero;
                }
                catch (EntryPointNotFoundException ex)
                {
                    reason = $"エントリポイントが見つかりません: {ex.Message}";
                    return IntPtr.Zero;
                }
                catch (BadImageFormatException ex)
                {
                    reason = $"DLL が不正な形式です（通常はアーキテクチャ不一致）: {ex.Message}";
                    return IntPtr.Zero;
                }
                catch (Exception ex)
                {
                    reason = $"予期せぬ例外: {ex.GetType().Name} - {ex.Message}";
                    return IntPtr.Zero;
                }
            }
            catch (Exception ex)
            {
                reason = $"事前チェックで例外: {ex.Message}";
                return IntPtr.Zero;
            }
        }

        /// <summary>
        /// ウィンドウをWPFに埋め込む
        /// </summary>
        private void BtnEmbed_Click(object sender, RoutedEventArgs e)
        {
            if (nativeHwnd != IntPtr.Zero && hwndHost == null)
            {
                try
                {
                    // 埋め込み前の事前チェック
                    if (!PreflightChecks.ValidateNativeWindowForEmbedding(nativeHwnd, out string reason))
                    {
                        StatusText.Text = $"埋め込み不可: {reason}";
                        return;
                    }

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

        ///===============================================================================================
        /// <summary>
        /// Windowの破棄ボタンが押されたとき
        /// </summary>
        /// <param name="sender"></param>
        /// <param name="e"></param>
        ///===============================================================================================
        private void BtnDestroy_Click(object sender, RoutedEventArgs e)
        {
            if (nativeHwnd != IntPtr.Zero)
            {
                pieGameHost.Stop();
                NativeInterop.SetPieTickCallback(null);
                NativeInterop.DestroyNativeWindow();
                NativeWindowPanel.Children.Clear();
                hwndHost?.Dispose();
                hwndHost = null;
                pieTickCallback = null;
                wasPieRunning = false;
                nativeHwnd = IntPtr.Zero;

                StatusText.Text = "ウィンドウを破棄しました";
                BtnEmbed.IsEnabled = false;
                BtnShow.IsEnabled = false;
            }
        }

        /// <summary>
        /// メインループのタイマーイベント
        /// </summary>
        private void MessageLoopTimer_Tick(object? sender, EventArgs e)
        {
            if (nativeHwnd != IntPtr.Zero)
            {
                NativeInterop.MessageLoopIteration();

                bool isPieRunning = NativeInterop.IsPieRunning();
                if (isPieRunning && !wasPieRunning)
                {
                    pieGameHost.Start();
                    StatusText.Text = "PIE を開始しました (C# Game Start)";
                }
                else if (!isPieRunning && wasPieRunning)
                {
                    pieGameHost.Stop();
                    StatusText.Text = "PIE を停止しました (C# Game Stop)";
                }
                wasPieRunning = isPieRunning;
            }
        }

        protected override void OnClosed(EventArgs e)
        {
            messageLoopTimer?.Stop();

            if (nativeHwnd != IntPtr.Zero)
            {
                pieGameHost.Stop();
                NativeInterop.SetPieTickCallback(null);
                NativeInterop.DestroyNativeWindow();
            }

            pieTickCallback = null;
            hwndHost?.Dispose();
            base.OnClosed(e);
        }

        private void OnPieTickFromNative(float deltaSeconds)
        {
            pieGameHost.Tick(deltaSeconds);
        }
    }

    // ネイティブウィンドウをWPFに埋め込むためのHwndHost
    public class NativeWindowHost : HwndHost
    {
        private IntPtr childHandle;
        private const int CW_USEDEFAULT = unchecked((int)0x80000000);
        private bool isDisposed = false;

        public NativeWindowHost(IntPtr hwnd)
        {
            childHandle = hwnd;
        }

        protected override HandleRef BuildWindowCore(HandleRef hwndParent)
        {
            // ネイティブウィンドウの親をWPFウィンドウに設定
            NativeInterop.SetParent(childHandle, hwndParent.Handle);

            // WPF 埋め込み時は子ウィンドウ向けスタイルへ正規化する。
            // WS_OVERLAPPEDWINDOW 系が残ると OpenGL 子ウィンドウで更新不良が起きることがある。
            const int childStyle = WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN;
            SetWindowLong(childHandle, GWL_STYLE, childStyle);


            // 表示
            NativeInterop.ShowNativeWindow();

            return new HandleRef(this, childHandle);
        }

        protected override void DestroyWindowCore(HandleRef hwnd)
        {
            // 親との関係を解除
            //NativeInterop.SetParent(hwnd.Handle, IntPtr.Zero);

            // ここでネイティブ側に破棄を依頼する（重要！）
            if (!isDisposed)
            {
                NativeInterop.DestroyNativeWindow();  // ← これを必ず呼ぶ
                isDisposed = true;
            }
        }

        protected override void OnWindowPositionChanged(Rect rcBounds)
        {
            if (childHandle != IntPtr.Zero)
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
                
        }

        [DllImport("user32.dll", SetLastError = true)]
        private static extern int GetWindowLong(IntPtr hWnd, int nIndex);

        [DllImport("user32.dll", SetLastError = true)]
        private static extern int SetWindowLong(IntPtr hWnd, int nIndex, int dwNewLong);

        private const int GWL_STYLE = -16;
        private const int WS_CHILD = 0x40000000;
        private const int WS_VISIBLE = 0x10000000;
        private const int WS_CLIPSIBLINGS = 0x04000000;
        private const int WS_CLIPCHILDREN = 0x02000000;
        private const int WS_POPUP = unchecked((int)0x80000000);
    }

    // 事前チェック用ユーティリティ
    internal static class PreflightChecks
    {
        [DllImport("user32.dll")]
        private static extern bool IsWindow(IntPtr hWnd);

        [DllImport("user32.dll")]
        private static extern bool IsWindowVisible(IntPtr hWnd);

        [DllImport("user32.dll")]
        private static extern uint GetWindowThreadProcessId(IntPtr hWnd, out uint lpdwProcessId);

        // ProgID ベースで COM が登録されているかチェック（速い事前確認）
        public static bool IsComProgIdRegistered(string progId)
        {
            try
            {
                return Type.GetTypeFromProgID(progId) != null;
            }
            catch
            {
                return false;
            }
        }

        // 埋め込み対象の HWND が妥当かどうか検証する簡易チェック
        public static bool ValidateNativeWindowForEmbedding(IntPtr hWnd, out string reason)
        {
            reason = string.Empty;

            if (hWnd == IntPtr.Zero)
            {
                reason = "HWND がゼロです";
                return false;
            }

            try
            {
                if (!IsWindow(hWnd))
                {
                    reason = "指定された HWND は有効なウィンドウではありません";
                    return false;
                }

                if (!IsWindowVisible(hWnd))
                {
                    // 非表示でも埋め込みできる場合があるが警告を出す
                    reason = "ウィンドウが非表示です（表示されない可能性があります）";
                    //return false;
                }

                uint pid;
                GetWindowThreadProcessId(hWnd, out pid);
                int currentPid = Process.GetCurrentProcess().Id;
                if (pid != 0 && pid != (uint)currentPid)
                {
                    // クロスプロセスのウィンドウを埋め込む場合の注意点
                    reason = $"ウィンドウは別プロセス (PID={pid}) に所属しています。クロスプロセス埋め込みは制限があり失敗する可能性があります";
                    return false;
                }
            }
            catch (Exception ex)
            {
                reason = $"検証中に例外: {ex.Message}";
                return false;
            }

            return true;
        }
    }

    // PE ヘッダ解析で DLL が 64bit かどうかを判定する最小実装
    internal static class PeUtils
    {
        public static bool IsDll64Bit(string path)
        {
            using (var fs = new FileStream(path, FileMode.Open, FileAccess.Read, FileShare.Read))
            using (var br = new BinaryReader(fs))
            {
                // DOS header e_lfanew at 0x3C
                fs.Seek(0x3C, SeekOrigin.Begin);
                int e_lfanew = br.ReadInt32();
                fs.Seek(e_lfanew + 4, SeekOrigin.Begin); // skip "PE\0\0"
                ushort machine = br.ReadUInt16(); // IMAGE_FILE_HEADER.Machine

                const ushort IMAGE_FILE_MACHINE_AMD64 = 0x8664;
                const ushort IMAGE_FILE_MACHINE_I386 = 0x014c;

                if (machine == IMAGE_FILE_MACHINE_AMD64) return true;
                if (machine == IMAGE_FILE_MACHINE_I386) return false;

                // 不明な値は既定でプロセスと同じビット幅を返す
                return Environment.Is64BitProcess;
            }
        }
    }
}
