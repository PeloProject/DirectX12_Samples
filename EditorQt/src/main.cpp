#include "EditorRuntimeHost.h"
#include "ImGuiEditorFrontend.h"
#include "LaunchOptions.h"
#include "QtEditorFrontend.h"
#include "RuntimeBridge.h"

#include <QApplication>

#include <Windows.h>


///=====================================================================
/// <summary>
/// メインエントリポイント
/// </summary>
/// <param name="argc"></param>
/// <param name="argv"></param>
/// <returns></returns>
///=====================================================================
int main(int argc, char* argv[])
{
    const LaunchOptions options = ParseLaunchOptions(argc, argv);
    const QString baseDir = GetEditorBaseDirectory();
    RuntimeBridge runtime;

    if (options.gameMode)
    {
        return RunStandaloneGameMode(runtime, baseDir);
    }

    if (!runtime.Load(baseDir))
    {
        MessageBoxW(nullptr,
            reinterpret_cast<LPCWSTR>(runtime.lastBridgeError().utf16()),
            L"Editor",
            MB_ICONERROR | MB_OK);
        return -1;
    }

    if (options.uiBackend == EditorUiBackend::ImGui)
    {
        ImGuiEditorFrontend frontend;
        frontend.bindRuntimeBridge(&runtime);
        if (!frontend.initialize(baseDir))
        {
            frontend.showError(runtime.lastBridgeError());
            return -2;
        }

        const int exitCode = frontend.run();
        frontend.shutdown();
        // NativeAOT DLL_PROCESS_DETACH が ExitProcess 時にデッドロックするのを防ぐため
        // TerminateProcess で強制終了する。レイアウト保存・レンダラシャットダウンは
        // frontend.run() 内で完了済み。
        TerminateProcess(GetCurrentProcess(), static_cast<UINT>(exitCode >= 0 ? exitCode : 0));
        return exitCode;
    }

    QApplication app(argc, argv);
    app.setQuitOnLastWindowClosed(true);
    QtEditorFrontend frontend(app);
    frontend.bindRuntimeBridge(&runtime);
    if (!frontend.initialize(baseDir))
    {
        frontend.showError(runtime.lastBridgeError());
        return -3;
    }

    const int exitCode = frontend.run();
    frontend.shutdown();
    // NativeAOT DLL_PROCESS_DETACH が ExitProcess 時にデッドロックするのを防ぐため
    // TerminateProcess で強制終了する。レイアウト保存・レンダラシャットダウンは
    // frontend.run() 内で完了済み。
    TerminateProcess(GetCurrentProcess(), static_cast<UINT>(exitCode >= 0 ? exitCode : 0));
    return exitCode;
}
