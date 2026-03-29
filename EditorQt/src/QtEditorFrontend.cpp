#include "QtEditorFrontend.h"

#include "MainWindow.h"
#include "RuntimeBridge.h"

#include <QApplication>
#include <QMessageBox>

QtEditorFrontend::QtEditorFrontend(QApplication& app)
    : app_(app)
{
}

QtEditorFrontend::~QtEditorFrontend()
{
    shutdown();
}

void QtEditorFrontend::bindRuntimeBridge(RuntimeBridge* runtime)
{
    runtime_ = runtime;
}

bool QtEditorFrontend::initialize(const QString&)
{
    if (runtime_ == nullptr)
    {
        return false;
    }

    window_ = new MainWindow();
    if (!window_->initialize(runtime_))
    {
        return false;
    }

    window_->show();
    return true;
}

int QtEditorFrontend::run()
{
    return app_.exec();
}

void QtEditorFrontend::shutdown()
{
    if (window_ != nullptr)
    {
        window_->close();
        delete window_;
        window_ = nullptr;
    }
}

void QtEditorFrontend::requestClose()
{
    if (window_ != nullptr)
    {
        window_->close();
    }
}

void QtEditorFrontend::showError(const QString& message)
{
    QMessageBox::critical(window_, QStringLiteral("Editor"), message);
}
