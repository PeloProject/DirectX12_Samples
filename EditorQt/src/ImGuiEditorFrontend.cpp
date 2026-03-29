#include "ImGuiEditorFrontend.h"

#include "RuntimeBridge.h"

#include <Windows.h>

void ImGuiEditorFrontend::bindRuntimeBridge(RuntimeBridge* runtime)
{
    runtime_ = runtime;
}

bool ImGuiEditorFrontend::initialize(const QString&)
{
    if (runtime_ == nullptr)
    {
        return false;
    }

    runtime_->setStandaloneMode(false);
    if (!runtime_->createNativeWindow(true))
    {
        return false;
    }

    runtime_->showNativeWindow();
    initialized_ = true;
    return true;
}

int ImGuiEditorFrontend::run()
{
    if (!initialized_ || runtime_ == nullptr)
    {
        return -1;
    }

    while (runtime_->isNativeWindowValid())
    {
        runtime_->tick();
        Sleep(16);
    }

    return 0;
}

void ImGuiEditorFrontend::shutdown()
{
    if (runtime_ != nullptr)
    {
        runtime_->destroyNativeWindow();
    }
    initialized_ = false;
}

void ImGuiEditorFrontend::requestClose()
{
    shutdown();
}

void ImGuiEditorFrontend::showError(const QString& message)
{
    MessageBoxW(nullptr,
        reinterpret_cast<LPCWSTR>(message.utf16()),
        L"Editor",
        MB_ICONERROR | MB_OK);
}
