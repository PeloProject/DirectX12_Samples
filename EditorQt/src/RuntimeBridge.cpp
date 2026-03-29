#include "RuntimeBridge.h"

#include <QDir>
#include <QFileInfo>

#ifdef _WIN32
#include <Windows.h>
#include <libloaderapi.h>
#endif

RuntimeBridge::RuntimeBridge() = default;

RuntimeBridge::~RuntimeBridge()
{
    unload();
}

bool RuntimeBridge::load(const QString& baseDir)
{
#ifdef _WIN32
    unload();

    const QString dllPath = QDir(baseDir).filePath(QStringLiteral("ApplicationDLL.dll"));
    if (!QFileInfo::exists(dllPath))
    {
        setLastError(QStringLiteral("ApplicationDLL.dll was not found: %1").arg(dllPath));
        return false;
    }

    module_ = LoadLibraryW(reinterpret_cast<LPCWSTR>(dllPath.utf16()));
    if (module_ == nullptr)
    {
        setLastError(QStringLiteral("Failed to load ApplicationDLL.dll."));
        return false;
    }

    bool ok = true;
    ok = resolve(createNativeWindow_, "CreateNativeWindow") && ok;
    ok = resolve(showNativeWindow_, "ShowNativeWindow") && ok;
    ok = resolve(hideNativeWindow_, "HideNativeWindow") && ok;
    ok = resolve(destroyNativeWindow_, "DestroyNativeWindow") && ok;
    ok = resolve(messageLoopIteration_, "MessageLoopIteration") && ok;
    ok = resolve(startPie_, "StartPie") && ok;
    ok = resolve(stopPie_, "StopPie") && ok;
    ok = resolve(setEditorUiEnabled_, "SetEditorUiEnabled") && ok;
    ok = resolve(setStandaloneMode_, "SetStandaloneMode") && ok;
    ok = resolve(isPieRunning_, "IsPieRunning") && ok;
    ok = resolve(setRendererBackend_, "SetRendererBackend") && ok;
    ok = resolve(getRendererBackend_, "GetRendererBackend") && ok;
    ok = resolve(getRuntimeStatusText_, "GetRuntimeStatusText") && ok;
    ok = resolve(getRuntimeLastErrorText_, "GetRuntimeLastErrorText") && ok;
    ok = resolve(getNativeWindowHandle_, "GetNativeWindowHandle") && ok;

    if (!ok)
    {
        unload();
        return false;
    }

    lastError_.clear();
    return true;
#else
    Q_UNUSED(baseDir);
    setLastError(QStringLiteral("RuntimeBridge is currently supported only on Windows."));
    return false;
#endif
}

bool RuntimeBridge::isLoaded() const
{
#ifdef _WIN32
    return module_ != nullptr;
#else
    return false;
#endif
}

bool RuntimeBridge::createNativeWindow(bool enableEditorUi)
{
#ifdef _WIN32
    if (!isLoaded())
    {
        setLastError(QStringLiteral("ApplicationDLL.dll is not loaded."));
        return false;
    }

    setEditorUiEnabled(enableEditorUi);
    if (createNativeWindow_() == nullptr)
    {
        setLastError(runtimeStatus());
        return false;
    }

    lastError_.clear();
    return true;
#else
    Q_UNUSED(enableEditorUi);
    return false;
#endif
}

void RuntimeBridge::destroyNativeWindow()
{
#ifdef _WIN32
    if (destroyNativeWindow_ != nullptr)
    {
        destroyNativeWindow_();
    }
#endif
}

void RuntimeBridge::showNativeWindow()
{
#ifdef _WIN32
    if (showNativeWindow_ != nullptr)
    {
        showNativeWindow_();
    }
#endif
}

void RuntimeBridge::hideNativeWindow()
{
#ifdef _WIN32
    if (hideNativeWindow_ != nullptr)
    {
        hideNativeWindow_();
    }
#endif
}

void RuntimeBridge::tick()
{
#ifdef _WIN32
    if (messageLoopIteration_ != nullptr)
    {
        messageLoopIteration_();
    }
#endif
}

void RuntimeBridge::startPie()
{
#ifdef _WIN32
    if (startPie_ != nullptr)
    {
        startPie_();
    }
#endif
}

void RuntimeBridge::stopPie()
{
#ifdef _WIN32
    if (stopPie_ != nullptr)
    {
        stopPie_();
    }
#endif
}

bool RuntimeBridge::isPieRunning() const
{
#ifdef _WIN32
    return isPieRunning_ != nullptr && isPieRunning_() != FALSE;
#else
    return false;
#endif
}

void RuntimeBridge::setStandaloneMode(bool enabled)
{
#ifdef _WIN32
    if (setStandaloneMode_ != nullptr)
    {
        setStandaloneMode_(enabled ? TRUE : FALSE);
    }
#else
    Q_UNUSED(enabled);
#endif
}

void RuntimeBridge::setEditorUiEnabled(bool enabled)
{
#ifdef _WIN32
    if (setEditorUiEnabled_ != nullptr)
    {
        setEditorUiEnabled_(enabled ? TRUE : FALSE);
    }
#else
    Q_UNUSED(enabled);
#endif
}

bool RuntimeBridge::setRendererBackend(RendererBackend backend)
{
#ifdef _WIN32
    return setRendererBackend_ != nullptr && setRendererBackend_(static_cast<unsigned int>(backend)) != FALSE;
#else
    Q_UNUSED(backend);
    return false;
#endif
}

RendererBackend RuntimeBridge::rendererBackend() const
{
#ifdef _WIN32
    if (getRendererBackend_ == nullptr)
    {
        return RendererBackend::DirectX12;
    }
    return static_cast<RendererBackend>(getRendererBackend_());
#else
    return RendererBackend::DirectX12;
#endif
}

QString RuntimeBridge::runtimeStatus() const
{
#ifdef _WIN32
    return fromUtf8(getRuntimeStatusText_ != nullptr ? getRuntimeStatusText_() : "");
#else
    return QString();
#endif
}

QString RuntimeBridge::runtimeLastError() const
{
#ifdef _WIN32
    return fromUtf8(getRuntimeLastErrorText_ != nullptr ? getRuntimeLastErrorText_() : "");
#else
    return QString();
#endif
}

QString RuntimeBridge::lastBridgeError() const
{
    return lastError_;
}

HWND RuntimeBridge::nativeWindowHandle() const
{
#ifdef _WIN32
    return getNativeWindowHandle_ != nullptr ? getNativeWindowHandle_() : nullptr;
#else
    return nullptr;
#endif
}

bool RuntimeBridge::isNativeWindowValid() const
{
#ifdef _WIN32
    const HWND hwnd = nativeWindowHandle();
    return hwnd != nullptr && IsWindow(hwnd) != FALSE;
#else
    return false;
#endif
}

template <typename T>
bool RuntimeBridge::resolve(T& fn, const char* name)
{
#ifdef _WIN32
    fn = reinterpret_cast<T>(GetProcAddress(module_, name));
    if (fn == nullptr)
    {
        setLastError(QStringLiteral("Failed to resolve export: %1").arg(QString::fromLatin1(name)));
        return false;
    }
    return true;
#else
    Q_UNUSED(fn);
    Q_UNUSED(name);
    return false;
#endif
}

QString RuntimeBridge::fromUtf8(const char* text) const
{
    return QString::fromUtf8(text != nullptr ? text : "");
}

void RuntimeBridge::unload()
{
#ifdef _WIN32
    if (module_ != nullptr)
    {
        FreeLibrary(module_);
        module_ = nullptr;
    }

    createNativeWindow_ = nullptr;
    showNativeWindow_ = nullptr;
    hideNativeWindow_ = nullptr;
    destroyNativeWindow_ = nullptr;
    messageLoopIteration_ = nullptr;
    startPie_ = nullptr;
    stopPie_ = nullptr;
    setEditorUiEnabled_ = nullptr;
    setStandaloneMode_ = nullptr;
    isPieRunning_ = nullptr;
    setRendererBackend_ = nullptr;
    getRendererBackend_ = nullptr;
    getRuntimeStatusText_ = nullptr;
    getRuntimeLastErrorText_ = nullptr;
    getNativeWindowHandle_ = nullptr;
#endif
}

void RuntimeBridge::setLastError(const QString& message)
{
    lastError_ = message;
}
