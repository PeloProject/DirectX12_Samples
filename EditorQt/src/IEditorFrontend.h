#pragma once

#include <QString>

class RuntimeBridge;

class IEditorFrontend
{
public:
    virtual ~IEditorFrontend() = default;

    virtual void bindRuntimeBridge(RuntimeBridge* runtime) = 0;
    virtual bool initialize(const QString& baseDir) = 0;
    virtual int run() = 0;
    virtual void shutdown() = 0;
    virtual void requestClose() = 0;
    virtual void showError(const QString& message) = 0;
};
