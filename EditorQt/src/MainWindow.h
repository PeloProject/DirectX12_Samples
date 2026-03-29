#pragma once

#include "RuntimeBridge.h"

#include <QListWidget>
#include <QMainWindow>

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
class QResizeEvent;
class QShowEvent;
class QTimer;
class QWidget;

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
    void setAssetDropHandler(std::function<void(const QString&)> handler);

protected:
    void resizeEvent(QResizeEvent* event) override;
    void showEvent(QShowEvent* event) override;
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dropEvent(QDropEvent* event) override;

private:
    void syncNativeWindow();

private:
    HWND hwnd_ = nullptr;
    std::function<void(const QString&)> assetDropHandler_;
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

    bool initialize(RuntimeBridge* runtime);

protected:
    void closeEvent(QCloseEvent* event) override;

private:
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
    void attachNativeViewport();
    void detachNativeViewport();
    QString backendName(RendererBackend backend) const;
    ads::CDockWidget* createDockWidget(const QString& title, const QString& objectName, QWidget* content, const QSize& minimumSize);

private:
    RuntimeBridge* runtime_ = nullptr;
    std::vector<EditorWorldActor> worldActors_;
    int nextSpawnedActorSerial_ = 1;
    QString lastRuntimeError_;

    ads::CDockManager* dockManager_ = nullptr;
    QWidget* toolsPanel_ = nullptr;
    NativeViewportHost* viewportHost_ = nullptr;
    ads::CDockWidget* toolsDock_ = nullptr;
    ads::CDockWidget* viewportDock_ = nullptr;
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
