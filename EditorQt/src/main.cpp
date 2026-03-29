#include "EditorRuntimeHost.h"
#include "ImGuiEditorFrontend.h"
#include "LaunchOptions.h"
#include "QtEditorFrontend.h"
#include "RuntimeBridge.h"

#include <QApplication>

#include <Windows.h>

int main(int argc, char* argv[])
{
    const LaunchOptions options = ParseLaunchOptions(argc, argv);
    const QString baseDir = GetEditorBaseDirectory();
    RuntimeBridge runtime;

    if (options.gameMode)
    {
        return RunStandaloneGameMode(runtime, baseDir);
    }

    if (!runtime.load(baseDir))
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
    return exitCode;
}
