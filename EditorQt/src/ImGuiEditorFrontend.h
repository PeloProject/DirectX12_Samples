#pragma once

#include "IEditorFrontend.h"

class ImGuiEditorFrontend final : public IEditorFrontend
{
public:
    void bindRuntimeBridge(RuntimeBridge* runtime) override;
    bool initialize(const QString& baseDir) override;
    int run() override;
    void shutdown() override;
    void requestClose() override;
    void showError(const QString& message) override;

private:
    RuntimeBridge* runtime_ = nullptr;
    bool initialized_ = false;
};
