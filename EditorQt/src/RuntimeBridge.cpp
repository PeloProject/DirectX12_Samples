#include "RuntimeBridge.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>

#ifdef _WIN32
#include <Windows.h>
#include <libloaderapi.h>
#endif

RuntimeBridge::RuntimeBridge() = default;

RuntimeBridge::~RuntimeBridge()
{
    Unload();
}

///================================================================================================
/// <summary>
/// Windows環境において、指定されたベースディレクトリからアプリケーションDLLをロードします。
/// </summary>
/// <param name="baseDir"></param>
/// <param name="instanceTag"></param>
/// <returns></returns>
///================================================================================================
bool RuntimeBridge::Load(const QString& baseDir, const QString& instanceTag)
{
#ifdef _WIN32
    Unload();

	// アプリケーションDLLのパスを構築
    const QString sourceDllPath = QDir(baseDir).filePath(QStringLiteral("ApplicationDLL.dll"));
    if (!QFileInfo::exists(sourceDllPath))
    {
        setLastError(QStringLiteral("ApplicationDLL.dll was not found: %1").arg(sourceDllPath));
        return false;
    }

    QString dllPath = sourceDllPath;
    copiedModulePath_.clear();
    if (!instanceTag.isEmpty())
    {
        copiedModulePath_ = makeModuleCopyPath(baseDir, instanceTag);
        if (copiedModulePath_.isEmpty())
        {
            setLastError(QStringLiteral("Failed to build a copied runtime module path for tag: %1").arg(instanceTag));
            return false;
        }

        QFile::remove(copiedModulePath_);
        if (!QFile::copy(sourceDllPath, copiedModulePath_))
        {
            copiedModulePath_.clear();
            setLastError(QStringLiteral("Failed to create runtime module copy: %1").arg(sourceDllPath));
            return false;
        }

        dllPath = copiedModulePath_;
    }

    module_ = LoadLibraryW(reinterpret_cast<LPCWSTR>(dllPath.utf16()));
    if (module_ == nullptr)
    {
        if (!copiedModulePath_.isEmpty())
        {
            QFile::remove(copiedModulePath_);
            copiedModulePath_.clear();
        }
        setLastError(QStringLiteral("Failed to load ApplicationDLL.dll: %1").arg(dllPath));
        return false;
    }

    bool ok = true;
    ok = resolve(createNativeWindow_, "CreateNativeWindow") && ok;
    ok = resolve(createNativeChildWindow_, "CreateNativeChildWindow") && ok;
    ok = resolve(showNativeWindow_, "ShowNativeWindow") && ok;
    ok = resolve(hideNativeWindow_, "HideNativeWindow") && ok;
    ok = resolve(destroyNativeWindow_, "DestroyNativeWindow") && ok;
    ok = resolve(messageLoopIteration_, "MessageLoopIteration") && ok;
    ok = resolve(startPie_, "StartPie") && ok;
    ok = resolve(stopPie_, "StopPie") && ok;
    ok = resolve(setEditorUiEnabled_, "SetEditorUiEnabled") && ok;
    ok = resolve(setStandaloneMode_, "SetStandaloneMode") && ok;
    ok = resolve(setSceneViewportCamera_, "SetSceneViewportCamera") && ok;
    ok = resolve(getSceneViewportCamera_, "GetSceneViewportCamera") && ok;
    ok = resolve(setSceneViewportRotation_, "SetSceneViewportRotation") && ok;
    ok = resolve(getSceneViewportRotation_, "GetSceneViewportRotation") && ok;
    ok = resolve(setGameViewportCamera_, "SetGameViewportCamera") && ok;
    ok = resolve(getGameViewportCamera_, "GetGameViewportCamera") && ok;
    ok = resolve(isPieRunning_, "IsPieRunning") && ok;
    ok = resolve(setRendererBackend_, "SetRendererBackend") && ok;
    ok = resolve(getRendererBackend_, "GetRendererBackend") && ok;
    ok = resolve(getRuntimeStatusText_, "GetRuntimeStatusText") && ok;
    ok = resolve(getRuntimeLastErrorText_, "GetRuntimeLastErrorText") && ok;
    ok = resolve(getNativeWindowHandle_, "GetNativeWindowHandle") && ok;
    ok = resolve(createGameNativeWindow_, "CreateGameNativeWindow") && ok;
    ok = resolve(createGameNativeChildWindow_, "CreateGameNativeChildWindow") && ok;
    ok = resolve(destroyGameNativeWindow_, "DestroyGameNativeWindow") && ok;
    ok = resolve(getGameNativeWindowHandle_, "GetGameNativeWindowHandle") && ok;

    if (!ok)
    {
        Unload();
        return false;
    }

    lastError_.clear();
    loadedModulePath_ = dllPath;
    return true;
#else
    Q_UNUSED(baseDir);
    Q_UNUSED(instanceTag);
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

bool RuntimeBridge::createNativeWindowInParent(HWND parentHwnd, unsigned int width, unsigned int height, bool enableEditorUi)
{
#ifdef _WIN32
    if (!isLoaded())
    {
        setLastError(QStringLiteral("ApplicationDLL.dll is not loaded."));
        return false;
    }

    setEditorUiEnabled(enableEditorUi);
    if (createNativeChildWindow_ == nullptr || createNativeChildWindow_(parentHwnd, width, height) == nullptr)
    {
        setLastError(runtimeStatus());
        return false;
    }

    lastError_.clear();
    return true;
#else
    Q_UNUSED(parentHwnd);
    Q_UNUSED(width);
    Q_UNUSED(height);
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

bool RuntimeBridge::createGameNativeWindow()
{
#ifdef _WIN32
    if (!isLoaded())
    {
        setLastError(QStringLiteral("ApplicationDLL.dll is not loaded."));
        return false;
    }

    if (createGameNativeWindow_ == nullptr || createGameNativeWindow_() == nullptr)
    {
        setLastError(runtimeStatus());
        return false;
    }

    lastError_.clear();
    return true;
#else
    return false;
#endif
}

bool RuntimeBridge::createGameNativeWindowInParent(HWND parentHwnd, unsigned int width, unsigned int height)
{
#ifdef _WIN32
    if (!isLoaded())
    {
        setLastError(QStringLiteral("ApplicationDLL.dll is not loaded."));
        return false;
    }

    if (createGameNativeChildWindow_ == nullptr || createGameNativeChildWindow_(parentHwnd, width, height) == nullptr)
    {
        setLastError(runtimeStatus());
        return false;
    }

    lastError_.clear();
    return true;
#else
    Q_UNUSED(parentHwnd);
    Q_UNUSED(width);
    Q_UNUSED(height);
    return false;
#endif
}

void RuntimeBridge::destroyGameNativeWindow()
{
#ifdef _WIN32
    if (destroyGameNativeWindow_ != nullptr)
    {
        destroyGameNativeWindow_();
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

void RuntimeBridge::setSceneViewportCamera(float centerX, float centerY, float zoom)
{
#ifdef _WIN32
    if (setSceneViewportCamera_ != nullptr)
    {
        setSceneViewportCamera_(centerX, centerY, zoom);
    }
#else
    Q_UNUSED(centerX);
    Q_UNUSED(centerY);
    Q_UNUSED(zoom);
#endif
}

void RuntimeBridge::getSceneViewportCamera(float& centerX, float& centerY, float& zoom) const
{
    centerX = 0.0f;
    centerY = 0.0f;
    zoom = 1.0f;
#ifdef _WIN32
    if (getSceneViewportCamera_ != nullptr)
    {
        getSceneViewportCamera_(&centerX, &centerY, &zoom);
    }
#endif
}

void RuntimeBridge::setSceneViewportRotation(float rotationDegrees)
{
#ifdef _WIN32
    if (setSceneViewportRotation_ != nullptr)
    {
        setSceneViewportRotation_(rotationDegrees);
    }
#else
    Q_UNUSED(rotationDegrees);
#endif
}

float RuntimeBridge::sceneViewportRotation() const
{
#ifdef _WIN32
    if (getSceneViewportRotation_ != nullptr)
    {
        return getSceneViewportRotation_();
    }
#endif
    return 0.0f;
}

void RuntimeBridge::setGameViewportCamera(float centerX, float centerY, float zoom)
{
#ifdef _WIN32
    if (setGameViewportCamera_ != nullptr)
    {
        setGameViewportCamera_(centerX, centerY, zoom);
    }
#else
    Q_UNUSED(centerX);
    Q_UNUSED(centerY);
    Q_UNUSED(zoom);
#endif
}

void RuntimeBridge::getGameViewportCamera(float& centerX, float& centerY, float& zoom) const
{
    centerX = 0.0f;
    centerY = 0.0f;
    zoom = 1.0f;
#ifdef _WIN32
    if (getGameViewportCamera_ != nullptr)
    {
        getGameViewportCamera_(&centerX, &centerY, &zoom);
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

HWND RuntimeBridge::gameNativeWindowHandle() const
{
#ifdef _WIN32
    return getGameNativeWindowHandle_ != nullptr ? getGameNativeWindowHandle_() : nullptr;
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

bool RuntimeBridge::isGameNativeWindowValid() const
{
#ifdef _WIN32
    const HWND hwnd = gameNativeWindowHandle();
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

void RuntimeBridge::Unload()
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
    createGameNativeWindow_ = nullptr;
    destroyGameNativeWindow_ = nullptr;
    getGameNativeWindowHandle_ = nullptr;

    loadedModulePath_.clear();
    if (!copiedModulePath_.isEmpty())
    {
        QFile::remove(copiedModulePath_);
        copiedModulePath_.clear();
    }
#endif
}

void RuntimeBridge::setLastError(const QString& message)
{
    lastError_ = message;
}

QString RuntimeBridge::makeModuleCopyPath(const QString& baseDir, const QString& instanceTag) const
{
    const QString sanitizedTag = QString(instanceTag).replace(QRegularExpression(QStringLiteral(R"([^A-Za-z0-9_\-])")), QStringLiteral("_"));
    if (sanitizedTag.isEmpty())
    {
        return QString();
    }

    return QDir(baseDir).filePath(QStringLiteral("ApplicationDLL.%1.dll").arg(sanitizedTag));
}
