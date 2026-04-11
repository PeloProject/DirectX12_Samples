#include "EditorRuntimeHost.h"

#include "RuntimeBridge.h"

#include <QDir>
#include <QFileInfo>

#include <Windows.h>

QString GetEditorBaseDirectory()
{
    wchar_t modulePath[MAX_PATH] = {};
    const DWORD length = GetModuleFileNameW(nullptr, modulePath, MAX_PATH);
    if (length == 0 || length == MAX_PATH)
    {
        return QDir::currentPath();
    }

    return QFileInfo(QString::fromWCharArray(modulePath, static_cast<int>(length))).absolutePath();
}

int RunStandaloneGameMode(RuntimeBridge& runtime, const QString& baseDir)
{
    if (!runtime.Load(baseDir))
    {
        MessageBoxW(nullptr,
            reinterpret_cast<LPCWSTR>(runtime.lastBridgeError().utf16()),
            L"Editor",
            MB_ICONERROR | MB_OK);
        return -1;
    }

    runtime.setStandaloneMode(true);
    if (!runtime.createNativeWindow(false))
    {
        MessageBoxW(nullptr,
            reinterpret_cast<LPCWSTR>(runtime.lastBridgeError().utf16()),
            L"Editor",
            MB_ICONERROR | MB_OK);
        return -2;
    }

    runtime.showNativeWindow();
    runtime.startPie();

    while (runtime.isNativeWindowValid())
    {
        runtime.tick();
        Sleep(16);
    }

    runtime.stopPie();
    runtime.destroyNativeWindow();
    return 0;
}
