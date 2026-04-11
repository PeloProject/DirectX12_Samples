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

    bool Load(const QString& baseDir, const QString& instanceTag = QString());
    bool isLoaded() const;

    bool createNativeWindow(bool enableEditorUi = false);
    bool createNativeWindowInParent(HWND parentHwnd, unsigned int width, unsigned int height, bool enableEditorUi = false);
    bool createGameNativeWindow();
    bool createGameNativeWindowInParent(HWND parentHwnd, unsigned int width, unsigned int height);
    void destroyNativeWindow();
    void destroyGameNativeWindow();
    void showNativeWindow();
    void hideNativeWindow();
    void tick();

    void startPie();
    void stopPie();
    bool isPieRunning() const;
    void setStandaloneMode(bool enabled);
    void setSceneViewportCamera(float centerX, float centerY, float zoom);
    void getSceneViewportCamera(float& centerX, float& centerY, float& zoom) const;
    void setSceneViewportRotation(float rotationDegrees);
    float sceneViewportRotation() const;
    void setGameViewportCamera(float centerX, float centerY, float zoom);
    void getGameViewportCamera(float& centerX, float& centerY, float& zoom) const;

    void setEditorUiEnabled(bool enabled);
    bool setRendererBackend(RendererBackend backend);
    RendererBackend rendererBackend() const;

    QString runtimeStatus() const;
    QString runtimeLastError() const;
    QString lastBridgeError() const;

    HWND nativeWindowHandle() const;
    HWND gameNativeWindowHandle() const;
    bool isNativeWindowValid() const;
    bool isGameNativeWindowValid() const;

private:
    template <typename T>
    bool resolve(T& fn, const char* name);

    QString fromUtf8(const char* text) const;
    void Unload();
    void setLastError(const QString& message);
    QString makeModuleCopyPath(const QString& baseDir, const QString& instanceTag) const;

private:
#ifdef _WIN32
    using CreateNativeWindowFn = HWND(__cdecl*)();
    using CreateNativeChildWindowFn = HWND(__cdecl*)(HWND, unsigned int, unsigned int);
    using VoidFn = void(__cdecl*)();
    using BoolFn = BOOL(__cdecl*)();
    using SetBoolFn = void(__cdecl*)(BOOL);
    using SetViewportCameraFn = void(__cdecl*)(float, float, float);
    using GetViewportCameraFn = void(__cdecl*)(float*, float*, float*);
    using SetFloatFn = void(__cdecl*)(float);
    using GetFloatFn = float(__cdecl*)();
    using SetRendererBackendFn = BOOL(__cdecl*)(unsigned int);
    using GetRendererBackendFn = unsigned int(__cdecl*)();
    using GetTextFn = const char*(__cdecl*)();
    using GetHwndFn = HWND(__cdecl*)();

    HMODULE module_ = nullptr;
    CreateNativeWindowFn createNativeWindow_ = nullptr;
    CreateNativeChildWindowFn createNativeChildWindow_ = nullptr;
    VoidFn showNativeWindow_ = nullptr;
    VoidFn hideNativeWindow_ = nullptr;
    VoidFn destroyNativeWindow_ = nullptr;
    VoidFn messageLoopIteration_ = nullptr;
    VoidFn startPie_ = nullptr;
    VoidFn stopPie_ = nullptr;
    SetBoolFn setEditorUiEnabled_ = nullptr;
    SetBoolFn setStandaloneMode_ = nullptr;
    SetViewportCameraFn setSceneViewportCamera_ = nullptr;
    GetViewportCameraFn getSceneViewportCamera_ = nullptr;
    SetFloatFn setSceneViewportRotation_ = nullptr;
    GetFloatFn getSceneViewportRotation_ = nullptr;
    SetViewportCameraFn setGameViewportCamera_ = nullptr;
    GetViewportCameraFn getGameViewportCamera_ = nullptr;
    BoolFn isPieRunning_ = nullptr;
    SetRendererBackendFn setRendererBackend_ = nullptr;
    GetRendererBackendFn getRendererBackend_ = nullptr;
    GetTextFn getRuntimeStatusText_ = nullptr;
    GetTextFn getRuntimeLastErrorText_ = nullptr;
    GetHwndFn getNativeWindowHandle_ = nullptr;
    CreateNativeWindowFn createGameNativeWindow_ = nullptr;
    CreateNativeChildWindowFn createGameNativeChildWindow_ = nullptr;
    VoidFn destroyGameNativeWindow_ = nullptr;
    GetHwndFn getGameNativeWindowHandle_ = nullptr;
#endif
    QString lastError_;
    QString loadedModulePath_;
    QString copiedModulePath_;
};
