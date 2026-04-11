#include "MainWindow.h"

#include <DockAreaWidget.h>
#include <DockManager.h>
#include <DockWidget.h>

#include <QAction>
#include <QCloseEvent>
#include <QComboBox>
#include <QCoreApplication>
#include <QDoubleSpinBox>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QGroupBox>
#include <QHideEvent>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QMimeData>
#include <QMouseEvent>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QResizeEvent>
#include <QSettings>
#include <QShowEvent>
#include <QTimer>
#include <QVBoxLayout>
#include <QWheelEvent>
#include <QWidget>

#include <algorithm>
#include <chrono>
#include <thread>

#ifdef _WIN32
#include <Windows.h>
#endif

namespace
{
    constexpr auto kAssetMimeType = "application/x-editor-asset-path";
    constexpr auto kLayoutVersion = 1;

    struct SampleAssetEntry
    {
        const char* assetPath = "";
    };

    constexpr SampleAssetEntry kSampleAssets[] =
    {
        { "Meshes/SM_Cube.asset" },
        { "Meshes/SM_Capsule.asset" },
        { "Meshes/SM_Cone.asset" },
        { "Lights/BP_PointLight.asset" },
        { "Cameras/BP_CineCamera.asset" }
    };

    QDoubleSpinBox* CreateTransformSpinBox(QWidget* parent, double step)
    {
        QDoubleSpinBox* spin = new QDoubleSpinBox(parent);
        spin->setRange(-100000.0, 100000.0);
        spin->setDecimals(3);
        spin->setSingleStep(step);
        return spin;
    }
}

AssetBrowserListWidget::AssetBrowserListWidget(QWidget* parent)
    : QListWidget(parent)
{
    setDragEnabled(true);
    setSelectionMode(QAbstractItemView::SingleSelection);
    setDefaultDropAction(Qt::CopyAction);
}

QMimeData* AssetBrowserListWidget::mimeData(const QList<QListWidgetItem*>& items) const
{
    QMimeData* mimeData = QListWidget::mimeData(items);
    if (items.isEmpty())
    {
        return mimeData;
    }

    mimeData->setData(kAssetMimeType, items.front()->text().toUtf8());
    mimeData->setText(items.front()->text());
    return mimeData;
}

NativeViewportHost::NativeViewportHost(QWidget* parent)
    : QWidget(parent)
{
    setAttribute(Qt::WA_NativeWindow, true);
    setAttribute(Qt::WA_PaintOnScreen, true);
    setMinimumSize(640, 360);
    setAcceptDrops(true);
    setFocusPolicy(Qt::StrongFocus);
}

void NativeViewportHost::setNativeWindow(HWND hwnd)
{
    hwnd_ = hwnd;
    syncNativeWindow();
}

void NativeViewportHost::clearNativeWindow()
{
    hwnd_ = nullptr;
}

HWND NativeViewportHost::nativeWindow() const
{
    return hwnd_;
}

void NativeViewportHost::setAssetDropHandler(std::function<void(const QString&)> handler)
{
    assetDropHandler_ = std::move(handler);
}

void NativeViewportHost::setPanHandler(std::function<void(const QPoint&)> handler)
{
    panHandler_ = std::move(handler);
}

void NativeViewportHost::setZoomHandler(std::function<void(float)> handler)
{
    zoomHandler_ = std::move(handler);
}

void NativeViewportHost::setOrbitHandler(std::function<void(const QPoint&)> handler)
{
    orbitHandler_ = std::move(handler);
}

void NativeViewportHost::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    syncNativeWindow();
}

void NativeViewportHost::showEvent(QShowEvent* event)
{
    QWidget::showEvent(event);
    syncNativeWindow();
}

void NativeViewportHost::hideEvent(QHideEvent* event)
{
    QWidget::hideEvent(event);
#ifdef _WIN32
    if (hwnd_ != nullptr)
    {
        const HWND parentHwnd = reinterpret_cast<HWND>(winId());
        if (hwnd_ != parentHwnd)
        {
            ShowWindow(hwnd_, SW_HIDE);
        }
    }
#endif
}

void NativeViewportHost::dragEnterEvent(QDragEnterEvent* event)
{
    if (event->mimeData()->hasFormat(kAssetMimeType) || event->mimeData()->hasText())
    {
        event->acceptProposedAction();
        return;
    }

    QWidget::dragEnterEvent(event);
}

void NativeViewportHost::dropEvent(QDropEvent* event)
{
    if (assetDropHandler_ != nullptr)
    {
        const QByteArray assetData = event->mimeData()->data(kAssetMimeType);
        const QString assetPath = assetData.isEmpty()
            ? event->mimeData()->text()
            : QString::fromUtf8(assetData);
        if (!assetPath.isEmpty())
        {
            assetDropHandler_(assetPath);
            event->acceptProposedAction();
            return;
        }
    }

    QWidget::dropEvent(event);
}

void NativeViewportHost::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::MiddleButton)
    {
        panning_ = true;
        lastMousePosition_ = event->position().toPoint();
        event->accept();
        return;
    }
    if (event->button() == Qt::RightButton)
    {
        orbiting_ = true;
        lastMousePosition_ = event->position().toPoint();
        event->accept();
        return;
    }

    QWidget::mousePressEvent(event);
}

void NativeViewportHost::mouseMoveEvent(QMouseEvent* event)
{
    if (panning_ && panHandler_ != nullptr)
    {
        const QPoint currentPosition = event->position().toPoint();
        panHandler_(currentPosition - lastMousePosition_);
        lastMousePosition_ = currentPosition;
        event->accept();
        return;
    }
    if (orbiting_ && orbitHandler_ != nullptr)
    {
        const QPoint currentPosition = event->position().toPoint();
        orbitHandler_(currentPosition - lastMousePosition_);
        lastMousePosition_ = currentPosition;
        event->accept();
        return;
    }

    QWidget::mouseMoveEvent(event);
}

void NativeViewportHost::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button() == Qt::MiddleButton)
    {
        panning_ = false;
        event->accept();
        return;
    }
    if (event->button() == Qt::RightButton)
    {
        orbiting_ = false;
        event->accept();
        return;
    }

    QWidget::mouseReleaseEvent(event);
}

void NativeViewportHost::wheelEvent(QWheelEvent* event)
{
    if (zoomHandler_ != nullptr && event->angleDelta().y() != 0)
    {
        zoomHandler_(event->angleDelta().y() > 0 ? 0.9f : 1.1f);
        event->accept();
        return;
    }

    QWidget::wheelEvent(event);
}

void NativeViewportHost::syncNativeWindow()
{
#ifdef _WIN32
    if (hwnd_ == nullptr)
    {
        return;
    }

    HWND parentHwnd = reinterpret_cast<HWND>(winId());
    if (parentHwnd == nullptr)
    {
        return;
    }

    if (!isVisible())
    {
        if (hwnd_ != parentHwnd)
        {
            ShowWindow(hwnd_, SW_HIDE);
        }
        return;
    }

    if (hwnd_ == parentHwnd)
    {
        return;
    }

    SetParent(hwnd_, parentHwnd);
    const LONG childStyle = WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN;
    SetWindowLongPtr(hwnd_, GWL_STYLE, childStyle);
    SetWindowLongPtr(hwnd_, GWL_EXSTYLE, 0);
    SetWindowPos(
        hwnd_,
        HWND_TOP,
        0,
        0,
        width(),
        height(),
        SWP_NOACTIVATE | SWP_FRAMECHANGED | SWP_SHOWWINDOW);
    UpdateWindow(hwnd_);
#endif
}

/// <summary>
/// 
/// </summary>
/// <param name="parent"></param>
MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    buildUi();
    buildMenu();
    connectSignals();

    tickTimer_ = new QTimer(this);
    tickTimer_->setInterval(16);
    connect(tickTimer_, &QTimer::timeout, this, [this]()
    {
        if (sceneRuntime_ != nullptr)
        {
            sceneRuntime_->tick();
            attachSceneViewport();
            attachGameViewport();
        }
        updateStatus();
    });

    populateWorkspace();
    updateStatus();
}

MainWindow::~MainWindow()
{
    detachSceneViewport();
    detachGameViewport();
    if (sceneRuntime_ != nullptr)
    {
        sceneRuntime_->destroyGameNativeWindow();
        sceneRuntime_->destroyNativeWindow();
    }
}

bool MainWindow::initialize(RuntimeBridge* runtime, const QString& baseDir)
{
    Q_UNUSED(baseDir);

    sceneRuntime_ = runtime;
    if (sceneRuntime_ == nullptr)
    {
        appendLogMessage(QStringLiteral("Runtime bridge is not available."), true);
        return false;
    }

    sceneRuntime_->setStandaloneMode(false);
    sceneRuntime_->setEditorUiEnabled(false);
    const bool sceneWindowCreated = embedNativeViewports_
        ? sceneRuntime_->createNativeWindowInParent(
            reinterpret_cast<HWND>(sceneViewportHost_->winId()),
            static_cast<unsigned int>((std::max)(1, sceneViewportHost_->width())),
            static_cast<unsigned int>((std::max)(1, sceneViewportHost_->height())),
            false)
        : sceneRuntime_->createNativeWindow(false);
    if (!sceneWindowCreated)
    {
        appendLogMessage(sceneRuntime_->lastBridgeError(), true);
        return false;
    }

    attachSceneViewport();
    if (embedNativeViewports_)
    {
        if (!sceneRuntime_->createGameNativeWindowInParent(
            reinterpret_cast<HWND>(gameViewportHost_->winId()),
            static_cast<unsigned int>((std::max)(1, gameViewportHost_->width())),
            static_cast<unsigned int>((std::max)(1, gameViewportHost_->height()))))
        {
            appendLogMessage(sceneRuntime_->lastBridgeError(), true);
            return false;
        }
    }
    else
    {
        sceneRuntime_->showNativeWindow();
    }
    attachGameViewport();
    syncRuntimeCamerasToEditor();
    tickTimer_->start();
    appendLogMessage(QStringLiteral("Shared Scene/Game runtime initialized."));
    updateStatus();
    return true;
}

void MainWindow::closeEvent(QCloseEvent* event)
{
    if (shuttingDown_)
    {
        event->accept();
        return;
    }
    shuttingDown_ = true;

    if (tickTimer_ != nullptr)
    {
        tickTimer_->stop();
    }

    if (dockManager_ != nullptr)
    {
        const auto floatingWidgets = dockManager_->floatingWidgets();
        for (ads::CFloatingDockContainer* floatingWidget : floatingWidgets)
        {
            if (floatingWidget != nullptr)
            {
                floatingWidget->close();
            }
        }
        dockManager_->hideManagerAndFloatingWidgets();
    }

    detachSceneViewport();
    detachGameViewport();
    if (sceneRuntime_ != nullptr)
    {
        sceneRuntime_->destroyGameNativeWindow();
        sceneRuntime_->destroyNativeWindow();
    }

    saveLayout();
    if (dockManager_ != nullptr)
    {
        delete dockManager_;
        dockManager_ = nullptr;
    }

    QMainWindow::closeEvent(event);
    QCoreApplication::quit();
}

void MainWindow::buildUi()
{
    setWindowTitle(QStringLiteral("Editor"));
    resize(1680, 980);

    ads::CDockManager::setConfigFlag(ads::CDockManager::OpaqueSplitterResize, true);
    ads::CDockManager::setConfigFlag(ads::CDockManager::XmlCompressionEnabled, false);
    ads::CDockManager::setConfigFlag(ads::CDockManager::FocusHighlighting, true);
    ads::CDockManager::setConfigFlag(ads::CDockManager::EqualSplitOnInsertion, true);
    ads::CDockManager::setConfigFlag(ads::CDockManager::FloatingContainerHasWidgetTitle, true);
    ads::CDockManager::setAutoHideConfigFlags(ads::CDockManager::DefaultAutoHideConfig);
    ads::CDockManager::setAutoHideConfigFlag(ads::CDockManager::AutoHideButtonCheckable, true);
    ads::CDockManager::setAutoHideConfigFlag(ads::CDockManager::AutoHideOpenOnDragHover, true);
    ads::CDockManager::setConfigParam(ads::CDockManager::AutoHideOpenOnDragHoverDelay_ms, 250);
    dockManager_ = new ads::CDockManager(this);

    toolsPanel_ = new QWidget(this);
    QHBoxLayout* toolsLayout = new QHBoxLayout(toolsPanel_);
    toolsLayout->setContentsMargins(6, 6, 6, 6);
    toolsLayout->setSpacing(8);

    createButton_ = new QPushButton(QStringLiteral("Create"), toolsPanel_);
    destroyButton_ = new QPushButton(QStringLiteral("Destroy"), toolsPanel_);
    showButton_ = new QPushButton(QStringLiteral("Show"), toolsPanel_);
    hideButton_ = new QPushButton(QStringLiteral("Hide"), toolsPanel_);
    playButton_ = new QPushButton(QStringLiteral("Play"), toolsPanel_);
    stopButton_ = new QPushButton(QStringLiteral("Stop"), toolsPanel_);
    backendCombo_ = new QComboBox(toolsPanel_);
    backendCombo_->addItem(QStringLiteral("DirectX12"), static_cast<int>(RendererBackend::DirectX12));
    backendCombo_->addItem(QStringLiteral("Vulkan"), static_cast<int>(RendererBackend::Vulkan));
    backendCombo_->addItem(QStringLiteral("OpenGL"), static_cast<int>(RendererBackend::OpenGL));

    toolsLayout->addWidget(createButton_);
    toolsLayout->addWidget(destroyButton_);
    toolsLayout->addWidget(showButton_);
    toolsLayout->addWidget(hideButton_);
    toolsLayout->addWidget(playButton_);
    toolsLayout->addWidget(stopButton_);
    toolsLayout->addWidget(new QLabel(QStringLiteral("Renderer"), toolsPanel_));
    toolsLayout->addWidget(backendCombo_);
    toolsLayout->addStretch(1);

    sceneViewportHost_ = new NativeViewportHost(this);
    sceneViewportHost_->setAssetDropHandler([this](const QString& assetPath)
    {
        spawnActorFromAssetPath(assetPath);
    });
    sceneViewportHost_->setPanHandler([this](const QPoint& delta)
    {
        if (sceneRuntime_ == nullptr || sceneViewportHost_->width() <= 0 || sceneViewportHost_->height() <= 0)
        {
            return;
        }

        float centerX = 0.0f;
        float centerY = 0.0f;
        float zoom = 1.0f;
        sceneRuntime_->getSceneViewportCamera(centerX, centerY, zoom);
        centerX -= (static_cast<float>(delta.x()) / static_cast<float>(sceneViewportHost_->width())) * 2.0f * zoom;
        centerY += (static_cast<float>(delta.y()) / static_cast<float>(sceneViewportHost_->height())) * 2.0f * zoom;
        sceneRuntime_->setSceneViewportCamera(centerX, centerY, zoom);
    });
    sceneViewportHost_->setZoomHandler([this](float factor)
    {
        if (sceneRuntime_ == nullptr)
        {
            return;
        }

        float centerX = 0.0f;
        float centerY = 0.0f;
        float zoom = 1.0f;
        sceneRuntime_->getSceneViewportCamera(centerX, centerY, zoom);
        zoom *= factor;
        if (zoom < 0.1f) zoom = 0.1f;
        if (zoom > 5.0f) zoom = 5.0f;
        sceneRuntime_->setSceneViewportCamera(centerX, centerY, zoom);
    });
    sceneViewportHost_->setOrbitHandler([this](const QPoint& delta)
    {
        if (sceneRuntime_ == nullptr)
        {
            return;
        }

        const float currentRotation = sceneRuntime_->sceneViewportRotation();
        sceneRuntime_->setSceneViewportRotation(currentRotation + static_cast<float>(delta.x()) * 0.35f);
    });
    gameViewportHost_ = new NativeViewportHost(this);
    gameViewportHost_->setAcceptDrops(false);

    outlinerList_ = new QListWidget(this);
    outlinerList_->setSelectionMode(QAbstractItemView::SingleSelection);

    QWidget* detailsPanel = new QWidget(this);
    QVBoxLayout* detailsLayout = new QVBoxLayout(detailsPanel);
    detailsLayout->setContentsMargins(6, 6, 6, 6);
    detailsHeaderLabel_ = new QLabel(detailsPanel);
    detailsSourceLabel_ = new QLabel(detailsPanel);
    detailsSourceLabel_->setWordWrap(true);
    detailsLayout->addWidget(detailsHeaderLabel_);
    detailsLayout->addWidget(detailsSourceLabel_);

    auto addTransformGroup = [&](const QString& title, QDoubleSpinBox* spins[3], double step)
    {
        QGroupBox* group = new QGroupBox(title, detailsPanel);
        QHBoxLayout* layout = new QHBoxLayout(group);
        for (int index = 0; index < 3; ++index)
        {
            spins[index] = CreateTransformSpinBox(group, step);
            layout->addWidget(spins[index]);
        }
        detailsLayout->addWidget(group);
    };

    addTransformGroup(QStringLiteral("Location"), locationSpin_, 0.1);
    addTransformGroup(QStringLiteral("Rotation"), rotationSpin_, 0.5);
    addTransformGroup(QStringLiteral("Scale"), scaleSpin_, 0.01);
    detailsLayout->addStretch(1);

    contentList_ = new AssetBrowserListWidget(this);
    logView_ = new QPlainTextEdit(this);
    logView_->setReadOnly(true);

    sceneViewportDock_ = createDockWidget(QStringLiteral("Scene"), QStringLiteral("SceneViewportDock"), sceneViewportHost_, QSize(900, 620));
    gameViewportDock_ = createDockWidget(QStringLiteral("Game"), QStringLiteral("GameViewportDock"), gameViewportHost_, QSize(900, 620));
    toolsDock_ = createDockWidget(QStringLiteral("Tools"), QStringLiteral("ToolsDock"), toolsPanel_, QSize(780, 92));
    outlinerDock_ = createDockWidget(QStringLiteral("Scene Hierarchy"), QStringLiteral("SceneHierarchyDock"), outlinerList_, QSize(300, 540));
    detailsDock_ = createDockWidget(QStringLiteral("Details"), QStringLiteral("DetailsDock"), detailsPanel, QSize(360, 560));
    contentDock_ = createDockWidget(QStringLiteral("Content Browser"), QStringLiteral("ContentBrowserDock"), contentList_, QSize(520, 260));
    logDock_ = createDockWidget(QStringLiteral("Log"), QStringLiteral("LogDock"), logView_, QSize(520, 260));

    ads::CDockAreaWidget* viewportArea = dockManager_->addDockWidget(ads::CenterDockWidgetArea, sceneViewportDock_);
    dockManager_->addDockWidgetTabToArea(gameViewportDock_, viewportArea);
    dockManager_->addDockWidget(ads::TopDockWidgetArea, toolsDock_, viewportArea);
    dockManager_->addDockWidget(ads::LeftDockWidgetArea, outlinerDock_, viewportArea);
    dockManager_->addDockWidget(ads::RightDockWidgetArea, detailsDock_, viewportArea);
    ads::CDockAreaWidget* contentArea = dockManager_->addDockWidget(ads::BottomDockWidgetArea, contentDock_, viewportArea);
    dockManager_->addDockWidget(ads::BottomDockWidgetArea, logDock_, contentArea);

    restoreLayout();
}

void MainWindow::buildMenu()
{
    QMenu* fileMenu = menuBar()->addMenu(QStringLiteral("&File"));
    fileMenu->addAction(QStringLiteral("New Level"));
    fileMenu->addAction(QStringLiteral("Save All"));
    fileMenu->addSeparator();
    fileMenu->addAction(QStringLiteral("Exit"), this, &QWidget::close);

    QMenu* playMenu = menuBar()->addMenu(QStringLiteral("&Play"));
    playAction_ = playMenu->addAction(QStringLiteral("Start PIE"));
    stopAction_ = playMenu->addAction(QStringLiteral("Stop PIE"));

    QMenu* windowMenu = menuBar()->addMenu(QStringLiteral("&Window"));
    windowMenu->addAction(toolsDock_->toggleViewAction());
    windowMenu->addAction(sceneViewportDock_->toggleViewAction());
    windowMenu->addAction(gameViewportDock_->toggleViewAction());
    windowMenu->addAction(outlinerDock_->toggleViewAction());
    windowMenu->addAction(detailsDock_->toggleViewAction());
    windowMenu->addAction(contentDock_->toggleViewAction());
    windowMenu->addAction(logDock_->toggleViewAction());

    QMenu* autoHideMenu = menuBar()->addMenu(QStringLiteral("&Auto Hide"));
    auto addAutoHideAction = [this, autoHideMenu](const QString& title, ads::CDockWidget* dock, ads::SideBarLocation side)
    {
        QAction* action = autoHideMenu->addAction(title);
        action->setCheckable(true);
        action->setChecked(dock->isAutoHide());
        connect(action, &QAction::triggered, this, [this, dock, side, action]()
        {
            dock->toggleAutoHide(side);
            action->setChecked(dock->isAutoHide());
            appendLogMessage(QStringLiteral("%1 %2.")
                .arg(dock->windowTitle())
                .arg(dock->isAutoHide() ? QStringLiteral("auto-hidden") : QStringLiteral("restored")));
        });
        connect(dock->toggleViewAction(), &QAction::changed, this, [dock, action]()
        {
            action->setChecked(dock->isAutoHide());
        });
    };

    addAutoHideAction(QStringLiteral("Tools"), toolsDock_, ads::SideBarTop);
    addAutoHideAction(QStringLiteral("Scene"), sceneViewportDock_, ads::SideBarLeft);
    addAutoHideAction(QStringLiteral("Game"), gameViewportDock_, ads::SideBarRight);
    addAutoHideAction(QStringLiteral("Scene Hierarchy"), outlinerDock_, ads::SideBarLeft);
    addAutoHideAction(QStringLiteral("Details"), detailsDock_, ads::SideBarRight);
    addAutoHideAction(QStringLiteral("Content Browser"), contentDock_, ads::SideBarBottom);
    addAutoHideAction(QStringLiteral("Log"), logDock_, ads::SideBarBottom);
}

void MainWindow::connectSignals()
{
    connect(createButton_, &QPushButton::clicked, this, [this]()
    {
        if (sceneRuntime_ == nullptr)
        {
            return;
        }

        if (!sceneRuntime_->isNativeWindowValid())
        {
            sceneRuntime_->setStandaloneMode(false);
            sceneRuntime_->setEditorUiEnabled(false);
            const bool createdSceneWindow = embedNativeViewports_
                ? sceneRuntime_->createNativeWindowInParent(
                    reinterpret_cast<HWND>(sceneViewportHost_->winId()),
                    static_cast<unsigned int>((std::max)(1, sceneViewportHost_->width())),
                    static_cast<unsigned int>((std::max)(1, sceneViewportHost_->height())),
                    false)
                : sceneRuntime_->createNativeWindow(false);
            if (!createdSceneWindow)
            {
                QMessageBox::warning(this, QStringLiteral("Editor"), sceneRuntime_->lastBridgeError());
                appendLogMessage(sceneRuntime_->lastBridgeError(), true);
                return;
            }
            attachSceneViewport();
        }

        sceneViewportDock_->toggleView(true);
        sceneViewportDock_->raise();

        if (embedNativeViewports_ && !sceneRuntime_->isGameNativeWindowValid())
        {
            const bool createdGameWindow = sceneRuntime_->createGameNativeWindowInParent(
                reinterpret_cast<HWND>(gameViewportHost_->winId()),
                static_cast<unsigned int>((std::max)(1, gameViewportHost_->width())),
                static_cast<unsigned int>((std::max)(1, gameViewportHost_->height())));
            if (!createdGameWindow)
            {
                QMessageBox::warning(this, QStringLiteral("Editor"), sceneRuntime_->lastBridgeError());
                appendLogMessage(sceneRuntime_->lastBridgeError(), true);
                return;
            }
            attachGameViewport();
        }

        appendLogMessage(QStringLiteral("Shared native window created."));
        updateStatus();
    });

    connect(destroyButton_, &QPushButton::clicked, this, [this]()
    {
        if (sceneRuntime_ == nullptr)
        {
            return;
        }

        detachSceneViewport();
        detachGameViewport();
        sceneRuntime_->destroyGameNativeWindow();
        sceneRuntime_->destroyNativeWindow();
        appendLogMessage(QStringLiteral("Shared native window destroyed."));
        updateStatus();
    });

    connect(showButton_, &QPushButton::clicked, this, [this]()
    {
        if (sceneRuntime_ != nullptr)
        {
            sceneRuntime_->showNativeWindow();
        }
        sceneViewportDock_->toggleView(true);
        sceneViewportDock_->showNormal();
        sceneViewportDock_->raise();
        gameViewportDock_->toggleView(true);
        gameViewportDock_->showNormal();
        gameViewportDock_->raise();
        appendLogMessage(QStringLiteral("Scene and Game viewports shown."));
        updateStatus();
    });

    connect(hideButton_, &QPushButton::clicked, this, [this]()
    {
        if (sceneRuntime_ != nullptr)
        {
            sceneRuntime_->hideNativeWindow();
        }
        sceneViewportDock_->toggleView(false);
        gameViewportDock_->toggleView(false);
        appendLogMessage(QStringLiteral("Shared viewport hidden."));
        updateStatus();
    });

    auto startPie = [this]()
    {
        if (sceneRuntime_ == nullptr)
        {
            return;
        }

        if (!sceneRuntime_->isNativeWindowValid())
        {
            sceneRuntime_->setStandaloneMode(false);
            sceneRuntime_->setEditorUiEnabled(false);
            const bool createdSceneWindow = embedNativeViewports_
                ? sceneRuntime_->createNativeWindowInParent(
                    reinterpret_cast<HWND>(sceneViewportHost_->winId()),
                    static_cast<unsigned int>((std::max)(1, sceneViewportHost_->width())),
                    static_cast<unsigned int>((std::max)(1, sceneViewportHost_->height())),
                    false)
                : sceneRuntime_->createNativeWindow(false);
            if (!createdSceneWindow)
            {
                appendLogMessage(sceneRuntime_->lastBridgeError(), true);
                updateStatus();
                return;
            }
        }
        if (embedNativeViewports_ && !sceneRuntime_->isGameNativeWindowValid())
        {
            if (!sceneRuntime_->createGameNativeWindowInParent(
                reinterpret_cast<HWND>(gameViewportHost_->winId()),
                static_cast<unsigned int>((std::max)(1, gameViewportHost_->width())),
                static_cast<unsigned int>((std::max)(1, gameViewportHost_->height()))))
            {
                appendLogMessage(sceneRuntime_->lastBridgeError(), true);
                updateStatus();
                return;
            }
        }
        if (!embedNativeViewports_)
        {
            sceneRuntime_->showNativeWindow();
        }
        attachSceneViewport();
        attachGameViewport();
        sceneRuntime_->startPie();
        appendLogMessage(QStringLiteral("PIE started on the shared Scene/Game runtime."));
        updateStatus();
    };

    auto stopPie = [this]()
    {
        if (sceneRuntime_ == nullptr)
        {
            return;
        }

        sceneRuntime_->stopPie();
        appendLogMessage(QStringLiteral("PIE stopped on the shared Scene/Game runtime."));
        updateStatus();
    };

    connect(playButton_, &QPushButton::clicked, this, startPie);
    connect(stopButton_, &QPushButton::clicked, this, stopPie);
    connect(playAction_, &QAction::triggered, this, startPie);
    connect(stopAction_, &QAction::triggered, this, stopPie);

    connect(backendCombo_, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int comboIndex)
    {
        if (sceneRuntime_ == nullptr)
        {
            return;
        }

        const QVariant backendValue = backendCombo_->itemData(comboIndex);
        if (!backendValue.isValid())
        {
            return;
        }

        const RendererBackend backend = static_cast<RendererBackend>(backendValue.toInt());
        sceneRuntime_->setRendererBackend(backend);
        appendLogMessage(QStringLiteral("Renderer switched to %1.").arg(backendName(backend)));
        updateStatus();
    });

    connect(outlinerList_, &QListWidget::currentRowChanged, this, [this]()
    {
        refreshActorDetails();
        updateStatus();
    });

    auto connectTransform = [this](QDoubleSpinBox* spins[3])
    {
        for (int axis = 0; axis < 3; ++axis)
        {
            connect(spins[axis], qOverload<double>(&QDoubleSpinBox::valueChanged), this, [this, spins]()
            {
                const int index = selectedActorIndex();
                if (index < 0)
                {
                    return;
                }

                EditorWorldActor& actor = worldActors_[static_cast<size_t>(index)];
                if (spins == locationSpin_)
                {
                    actor.location[0] = static_cast<float>(locationSpin_[0]->value());
                    actor.location[1] = static_cast<float>(locationSpin_[1]->value());
                    actor.location[2] = static_cast<float>(locationSpin_[2]->value());
                }
                else if (spins == rotationSpin_)
                {
                    actor.rotation[0] = static_cast<float>(rotationSpin_[0]->value());
                    actor.rotation[1] = static_cast<float>(rotationSpin_[1]->value());
                    actor.rotation[2] = static_cast<float>(rotationSpin_[2]->value());
                }
                else if (spins == scaleSpin_)
                {
                    actor.scale[0] = static_cast<float>(scaleSpin_[0]->value());
                    actor.scale[1] = static_cast<float>(scaleSpin_[1]->value());
                    actor.scale[2] = static_cast<float>(scaleSpin_[2]->value());
                }

                if (selectedActorIsMainCamera())
                {
                    applyMainCameraActorToRuntime();
                }
            });
        }
    };

    connectTransform(locationSpin_);
    connectTransform(rotationSpin_);
    connectTransform(scaleSpin_);
}

void MainWindow::populateWorkspace()
{
    worldActors_.clear();
    contentList_->clear();

    EditorWorldActor directionalLight;
    directionalLight.actorName = QStringLiteral("DirectionalLight");
    directionalLight.sourceAssetPath = QStringLiteral("Lights/BP_DirectionalLight.asset");
    directionalLight.rotation[0] = -35.0f;
    directionalLight.rotation[1] = 40.0f;
    worldActors_.push_back(directionalLight);

    EditorWorldActor mainCamera;
    mainCamera.actorName = QStringLiteral("MainCamera");
    mainCamera.sourceAssetPath = QStringLiteral("Cameras/BP_Camera.asset");
    mainCamera.location[2] = -250.0f;
    worldActors_.push_back(mainCamera);

    for (const SampleAssetEntry& asset : kSampleAssets)
    {
        contentList_->addItem(QString::fromUtf8(asset.assetPath));
    }

    refreshActorHierarchy();
    refreshActorDetails();
    appendLogMessage(QStringLiteral("Qt Advanced Docking System is active. Scene and Game viewports can be floated and docked independently."));
    appendLogMessage(QStringLiteral("Use the pin button on a dock title bar, or the Auto Hide menu, to collapse side panels like Visual Studio."));
}

void MainWindow::refreshActorHierarchy()
{
    const int currentSelection = selectedActorIndex();
    outlinerList_->clear();
    for (const EditorWorldActor& actor : worldActors_)
    {
        outlinerList_->addItem(actor.actorName);
    }

    if (!worldActors_.empty())
    {
        const int targetIndex = (std::min)(currentSelection >= 0 ? currentSelection : 0, static_cast<int>(worldActors_.size()) - 1);
        outlinerList_->setCurrentRow(targetIndex);
    }
}

void MainWindow::refreshActorDetails()
{
    const int index = selectedActorIndex();
    const bool hasSelection = index >= 0;

    detailsHeaderLabel_->setText(hasSelection
        ? QStringLiteral("Selected: %1").arg(worldActors_[static_cast<size_t>(index)].actorName)
        : QStringLiteral("Selected: (none)"));
    detailsSourceLabel_->setText(hasSelection
        ? QStringLiteral("Source: %1").arg(worldActors_[static_cast<size_t>(index)].sourceAssetPath)
        : QStringLiteral("Source: -"));

    auto assignTransform = [hasSelection](QDoubleSpinBox* spins[3], const float values[3], const float fallback[3])
    {
        for (int axis = 0; axis < 3; ++axis)
        {
            spins[axis]->blockSignals(true);
            spins[axis]->setEnabled(hasSelection);
            spins[axis]->setValue(hasSelection ? values[axis] : fallback[axis]);
            spins[axis]->blockSignals(false);
        }
    };

    const float zero[3] = { 0.0f, 0.0f, 0.0f };
    const float one[3] = { 1.0f, 1.0f, 1.0f };
    assignTransform(locationSpin_, hasSelection ? worldActors_[static_cast<size_t>(index)].location : zero, zero);
    assignTransform(rotationSpin_, hasSelection ? worldActors_[static_cast<size_t>(index)].rotation : zero, zero);
    assignTransform(scaleSpin_, hasSelection ? worldActors_[static_cast<size_t>(index)].scale : one, one);
}

void MainWindow::appendLogMessage(const QString& message, bool isError)
{
    if (logView_ == nullptr || message.isEmpty())
    {
        return;
    }

    const QString prefix = isError ? QStringLiteral("[Error] ") : QStringLiteral("[Info] ");
    logView_->appendPlainText(prefix + message);
}

void MainWindow::spawnActorFromAssetPath(const QString& assetPath)
{
    if (assetPath.isEmpty())
    {
        return;
    }

    EditorWorldActor actor;
    actor.actorName = makeSpawnActorName(assetPath);
    actor.sourceAssetPath = assetPath;
    actor.location[0] = static_cast<float>(nextSpawnedActorSerial_ - 2) * 25.0f;

    worldActors_.push_back(actor);
    refreshActorHierarchy();
    outlinerList_->setCurrentRow(static_cast<int>(worldActors_.size()) - 1);
    appendLogMessage(QStringLiteral("Spawned actor: %1 (from %2)").arg(actor.actorName, actor.sourceAssetPath));
    updateStatus();
}

int MainWindow::selectedActorIndex() const
{
    return outlinerList_ != nullptr ? outlinerList_->currentRow() : -1;
}

QString MainWindow::makeSpawnActorName(const QString& assetPath)
{
    QString baseName = assetPath;
    const int slashIndex = (std::max)(baseName.lastIndexOf('/'), baseName.lastIndexOf('\\'));
    if (slashIndex >= 0)
    {
        baseName = baseName.mid(slashIndex + 1);
    }
    const int dotIndex = baseName.lastIndexOf('.');
    if (dotIndex > 0)
    {
        baseName = baseName.left(dotIndex);
    }
    if (baseName.isEmpty())
    {
        baseName = QStringLiteral("Actor");
    }

    const QString actorName = QStringLiteral("%1_%2").arg(baseName).arg(nextSpawnedActorSerial_, 2, 10, QChar('0'));
    ++nextSpawnedActorSerial_;
    return actorName;
}

void MainWindow::updateStatus()
{
    const RendererBackend backend = sceneRuntime_ != nullptr ? sceneRuntime_->rendererBackend() : RendererBackend::DirectX12;
    const QString sceneRuntimeError = sceneRuntime_ != nullptr ? sceneRuntime_->runtimeLastError() : QString();
    const QString runtimeError = sceneRuntimeError;
    if (!runtimeError.isEmpty() && runtimeError != lastRuntimeError_)
    {
        appendLogMessage(runtimeError, true);
    }
    lastRuntimeError_ = runtimeError;

    setWindowTitle(QStringLiteral("Editor | Runtime: %1 | PIE: %2 | Renderer: %3 | Scene Actors: %4")
        .arg(sceneRuntime_ != nullptr ? sceneRuntime_->runtimeStatus() : QStringLiteral("Runtime unavailable"))
        .arg(sceneRuntime_ != nullptr && sceneRuntime_->isPieRunning() ? QStringLiteral("Running") : QStringLiteral("Stopped"))
        .arg(backendName(backend))
        .arg(worldActors_.size()));
}

void MainWindow::attachSceneViewport()
{
    if (!embedNativeViewports_)
    {
        return;
    }
    if (sceneRuntime_ != nullptr)
    {
        const HWND hwnd = sceneRuntime_->nativeWindowHandle();
        if (sceneViewportHost_->nativeWindow() != hwnd)
        {
            sceneViewportHost_->setNativeWindow(hwnd);
        }
    }
}

void MainWindow::detachSceneViewport()
{
    sceneViewportHost_->clearNativeWindow();
}

void MainWindow::attachGameViewport()
{
    if (!embedNativeViewports_)
    {
        return;
    }
    if (sceneRuntime_ != nullptr)
    {
        const HWND hwnd = sceneRuntime_->gameNativeWindowHandle();
        if (gameViewportHost_->nativeWindow() != hwnd)
        {
            gameViewportHost_->setNativeWindow(hwnd);
        }
    }
}

void MainWindow::detachGameViewport()
{
    gameViewportHost_->clearNativeWindow();
}

void MainWindow::syncRuntimeCamerasToEditor()
{
    if (sceneRuntime_ == nullptr)
    {
        return;
    }

    float gameCenterX = 0.0f;
    float gameCenterY = 0.0f;
    float gameZoom = 1.0f;
    sceneRuntime_->getGameViewportCamera(gameCenterX, gameCenterY, gameZoom);

    for (EditorWorldActor& actor : worldActors_)
    {
        if (actor.actorName == QStringLiteral("MainCamera"))
        {
            actor.location[0] = gameCenterX;
            actor.location[1] = gameCenterY;
            actor.location[2] = cameraDepthFromZoom(gameZoom);
            actor.scale[0] = gameZoom;
            actor.scale[1] = gameZoom;
            actor.scale[2] = 1.0f;
            break;
        }
    }

    refreshActorDetails();
}

void MainWindow::applyMainCameraActorToRuntime()
{
    if (sceneRuntime_ == nullptr)
    {
        return;
    }

    for (const EditorWorldActor& actor : worldActors_)
    {
        if (actor.actorName == QStringLiteral("MainCamera"))
        {
            sceneRuntime_->setGameViewportCamera(
                actor.location[0],
                actor.location[1],
                zoomFromCameraDepth(actor.location[2]));
            return;
        }
    }
}

bool MainWindow::selectedActorIsMainCamera() const
{
    const int index = selectedActorIndex();
    return index >= 0 && worldActors_[static_cast<size_t>(index)].actorName == QStringLiteral("MainCamera");
}

float MainWindow::zoomFromCameraDepth(float z)
{
    const float depth = std::abs(z) > 1.0f ? std::abs(z) : 250.0f;
    const float zoom = depth / 250.0f;
    return zoom < 0.1f ? 0.1f : zoom;
}

float MainWindow::cameraDepthFromZoom(float zoom)
{
    const float safeZoom = zoom < 0.1f ? 0.1f : zoom;
    return -250.0f * safeZoom;
}

bool MainWindow::ensureRuntimeBackend(RendererBackend backend, int maxAttempts)
{
    if (sceneRuntime_ == nullptr)
    {
        return false;
    }

    auto syncCombo = [this, backend]()
    {
        const int comboIndex = backendCombo_->findData(static_cast<int>(backend));
        if (comboIndex >= 0 && backendCombo_->currentIndex() != comboIndex)
        {
            backendCombo_->blockSignals(true);
            backendCombo_->setCurrentIndex(comboIndex);
            backendCombo_->blockSignals(false);
        }
    };

    if (sceneRuntime_->rendererBackend() == backend)
    {
        syncCombo();
        return true;
    }

    if (!sceneRuntime_->setRendererBackend(backend))
    {
        appendLogMessage(QStringLiteral("Renderer switch request failed: %1").arg(backendName(backend)), true);
        return false;
    }

    for (int attempt = 0; attempt < maxAttempts; ++attempt)
    {
        sceneRuntime_->tick();
        QCoreApplication::processEvents();
        if (sceneRuntime_->rendererBackend() == backend)
        {
            syncCombo();
            appendLogMessage(QStringLiteral("Shared runtime renderer is %1.").arg(backendName(backend)));
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }

    appendLogMessage(
        QStringLiteral("Renderer switch did not complete. Requested %1, current %2.")
            .arg(backendName(backend))
            .arg(backendName(sceneRuntime_->rendererBackend())),
        true);
    return false;
}

QString MainWindow::backendName(RendererBackend backend) const
{
    switch (backend)
    {
    case RendererBackend::DirectX12:
        return QStringLiteral("DirectX12");
    case RendererBackend::Vulkan:
        return QStringLiteral("Vulkan");
    case RendererBackend::OpenGL:
        return QStringLiteral("OpenGL");
    default:
        return QStringLiteral("Unknown");
    }
}

ads::CDockWidget* MainWindow::createDockWidget(const QString& title, const QString& objectName, QWidget* content, const QSize& minimumSize)
{
    ads::CDockWidget* dock = dockManager_->createDockWidget(title);
    dock->setObjectName(objectName);
    dock->setWidget(content, ads::CDockWidget::ForceNoScrollArea);
    dock->setMinimumSizeHintMode(ads::CDockWidget::MinimumSizeHintFromDockWidget);
    if (minimumSize.isValid())
    {
        dock->setMinimumSize(minimumSize);
        dock->resize(minimumSize);
    }
    return dock;
}

void MainWindow::restoreLayout()
{
    if (dockManager_ == nullptr)
    {
        return;
    }

    QCoreApplication::setOrganizationName(QStringLiteral("DirectX12Samples"));
    QCoreApplication::setApplicationName(QStringLiteral("Editor"));
    QSettings settings;

    const QByteArray geometry = settings.value(QStringLiteral("MainWindow/Geometry")).toByteArray();
    if (!geometry.isEmpty())
    {
        restoreGeometry(geometry);
    }

    const QByteArray dockState = settings.value(QStringLiteral("MainWindow/AdsState")).toByteArray();
    if (!dockState.isEmpty())
    {
        dockManager_->restoreState(dockState, kLayoutVersion);
    }
}

void MainWindow::saveLayout() const
{
    if (dockManager_ == nullptr)
    {
        return;
    }

    QCoreApplication::setOrganizationName(QStringLiteral("DirectX12Samples"));
    QCoreApplication::setApplicationName(QStringLiteral("Editor"));
    QSettings settings;
    settings.setValue(QStringLiteral("MainWindow/Geometry"), saveGeometry());
    settings.setValue(QStringLiteral("MainWindow/AdsState"), dockManager_->saveState(kLayoutVersion));
    settings.sync();
}
