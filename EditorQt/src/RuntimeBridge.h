#pragma once

#include <QString>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>
#else
using HWND = void*;
using BOOL = int;
#endif

enum class RendererBackend : unsigned int
{
    DirectX12 = 0,
    Vulkan = 1,
    OpenGL = 2,
};

class RuntimeBridge
{
public:
    RuntimeBridge();
    ~RuntimeBridge();

    bool load(const QString& baseDir);
    bool isLoaded() const;

    bool createNativeWindow(bool enableEditorUi = false);
    void destroyNativeWindow();
    void showNativeWindow();
    void hideNativeWindow();
    void tick();

    void startPie();
    void stopPie();
    bool isPieRunning() const;
    void setStandaloneMode(bool enabled);

    void setEditorUiEnabled(bool enabled);
    bool setRendererBackend(RendererBackend backend);
    RendererBackend rendererBackend() const;

    QString runtimeStatus() const;
    QString runtimeLastError() const;
    QString lastBridgeError() const;

    HWND nativeWindowHandle() const;
    bool isNativeWindowValid() const;

private:
    template <typename T>
    bool resolve(T& fn, const char* name);

    QString fromUtf8(const char* text) const;
    void unload();
    void setLastError(const QString& message);

private:
#ifdef _WIN32
    using CreateNativeWindowFn = HWND(__cdecl*)();
    using VoidFn = void(__cdecl*)();
    using BoolFn = BOOL(__cdecl*)();
    using SetBoolFn = void(__cdecl*)(BOOL);
    using SetRendererBackendFn = BOOL(__cdecl*)(unsigned int);
    using GetRendererBackendFn = unsigned int(__cdecl*)();
    using GetTextFn = const char*(__cdecl*)();
    using GetHwndFn = HWND(__cdecl*)();

    HMODULE module_ = nullptr;
    CreateNativeWindowFn createNativeWindow_ = nullptr;
    VoidFn showNativeWindow_ = nullptr;
    VoidFn hideNativeWindow_ = nullptr;
    VoidFn destroyNativeWindow_ = nullptr;
    VoidFn messageLoopIteration_ = nullptr;
    VoidFn startPie_ = nullptr;
    VoidFn stopPie_ = nullptr;
    SetBoolFn setEditorUiEnabled_ = nullptr;
    SetBoolFn setStandaloneMode_ = nullptr;
    BoolFn isPieRunning_ = nullptr;
    SetRendererBackendFn setRendererBackend_ = nullptr;
    GetRendererBackendFn getRendererBackend_ = nullptr;
    GetTextFn getRuntimeStatusText_ = nullptr;
    GetTextFn getRuntimeLastErrorText_ = nullptr;
    GetHwndFn getNativeWindowHandle_ = nullptr;
#endif
    QString lastError_;
};
