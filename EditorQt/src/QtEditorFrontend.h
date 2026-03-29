#pragma once

#include "IEditorFrontend.h"

#include <QPointer>

class QApplication;
class MainWindow;

class QtEditorFrontend final : public IEditorFrontend
{
public:
    explicit QtEditorFrontend(QApplication& app);
    ~QtEditorFrontend() override;

    void bindRuntimeBridge(RuntimeBridge* runtime) override;
    bool initialize(const QString& baseDir) override;
    int run() override;
    void shutdown() override;
    void requestClose() override;
    void showError(const QString& message) override;

private:
    QApplication& app_;
    RuntimeBridge* runtime_ = nullptr;
    QPointer<MainWindow> window_;
};
