#pragma once

#include "RuntimeBridge.h"

#include <QListWidget>
#include <QMainWindow>
#include <QPoint>

#include <functional>
#include <vector>

class QAction;
class QCloseEvent;
class QComboBox;
class QDoubleSpinBox;
class QDragEnterEvent;
class QDropEvent;
class QLabel;
class QMimeData;
class QPlainTextEdit;
class QPushButton;
class QHideEvent;
class QMouseEvent;
class QResizeEvent;
class QShowEvent;
class QTimer;
class QWidget;
class QWheelEvent;

namespace ads
{
    class CDockManager;
    class CDockWidget;
}

class AssetBrowserListWidget final : public QListWidget
{
public:
    explicit AssetBrowserListWidget(QWidget* parent = nullptr);

protected:
    QMimeData* mimeData(const QList<QListWidgetItem*>& items) const override;
};

class NativeViewportHost final : public QWidget
{
public:
    explicit NativeViewportHost(QWidget* parent = nullptr);

    void setNativeWindow(HWND hwnd);
    void clearNativeWindow();
    HWND nativeWindow() const;
    void setAssetDropHandler(std::function<void(const QString&)> handler);
    void setPanHandler(std::function<void(const QPoint&)> handler);
    void setZoomHandler(std::function<void(float)> handler);
    void setOrbitHandler(std::function<void(const QPoint&)> handler);

protected:
    void resizeEvent(QResizeEvent* event) override;
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dropEvent(QDropEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;

private:
    void syncNativeWindow();

private:
    HWND hwnd_ = nullptr;
    std::function<void(const QString&)> assetDropHandler_;
    std::function<void(const QPoint&)> panHandler_;
    std::function<void(float)> zoomHandler_;
    std::function<void(const QPoint&)> orbitHandler_;
    bool panning_ = false;
    bool orbiting_ = false;
    QPoint lastMousePosition_;
};

struct EditorWorldActor
{
    QString actorName;
    QString sourceAssetPath;
    float location[3] = { 0.0f, 0.0f, 0.0f };
    float rotation[3] = { 0.0f, 0.0f, 0.0f };
    float scale[3] = { 1.0f, 1.0f, 1.0f };
};

class MainWindow final : public QMainWindow
{
public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

    bool initialize(RuntimeBridge* runtime, const QString& baseDir);

protected:
    void closeEvent(QCloseEvent* event) override;

private:
    void restoreLayout();
    void saveLayout() const;
    void buildUi();
    void buildMenu();
    void connectSignals();
    void populateWorkspace();
    void refreshActorHierarchy();
    void refreshActorDetails();
    void appendLogMessage(const QString& message, bool isError = false);
    void spawnActorFromAssetPath(const QString& assetPath);
    int selectedActorIndex() const;
    QString makeSpawnActorName(const QString& assetPath);
    void updateStatus();
    void attachSceneViewport();
    void detachSceneViewport();
    void attachGameViewport();
    void detachGameViewport();
    bool ensureRuntimeBackend(RendererBackend backend, int maxAttempts = 24);
    QString backendName(RendererBackend backend) const;
    ads::CDockWidget* createDockWidget(const QString& title, const QString& objectName, QWidget* content, const QSize& minimumSize);
    void syncRuntimeCamerasToEditor();
    void applyMainCameraActorToRuntime();
    bool selectedActorIsMainCamera() const;
    static float zoomFromCameraDepth(float z);
    static float cameraDepthFromZoom(float zoom);

private:
    bool shuttingDown_ = false;
    bool embedNativeViewports_ = true;
    RuntimeBridge* sceneRuntime_ = nullptr;
    std::vector<EditorWorldActor> worldActors_;
    int nextSpawnedActorSerial_ = 1;
    QString lastRuntimeError_;

    ads::CDockManager* dockManager_ = nullptr;
    QWidget* toolsPanel_ = nullptr;
    NativeViewportHost* sceneViewportHost_ = nullptr;
    NativeViewportHost* gameViewportHost_ = nullptr;
    ads::CDockWidget* toolsDock_ = nullptr;
    ads::CDockWidget* sceneViewportDock_ = nullptr;
    ads::CDockWidget* gameViewportDock_ = nullptr;
    ads::CDockWidget* outlinerDock_ = nullptr;
    ads::CDockWidget* detailsDock_ = nullptr;
    ads::CDockWidget* contentDock_ = nullptr;
    ads::CDockWidget* logDock_ = nullptr;

    QListWidget* outlinerList_ = nullptr;
    AssetBrowserListWidget* contentList_ = nullptr;
    QPlainTextEdit* logView_ = nullptr;
    QLabel* detailsHeaderLabel_ = nullptr;
    QLabel* detailsSourceLabel_ = nullptr;
    QDoubleSpinBox* locationSpin_[3] = {};
    QDoubleSpinBox* rotationSpin_[3] = {};
    QDoubleSpinBox* scaleSpin_[3] = {};
    QComboBox* backendCombo_ = nullptr;
    QPushButton* createButton_ = nullptr;
    QPushButton* destroyButton_ = nullptr;
    QPushButton* showButton_ = nullptr;
    QPushButton* hideButton_ = nullptr;
    QPushButton* playButton_ = nullptr;
    QPushButton* stopButton_ = nullptr;
    QTimer* tickTimer_ = nullptr;

    QAction* playAction_ = nullptr;
    QAction* stopAction_ = nullptr;
};
