#pragma once

#include "IRenderDevice.h"
#include "DirectXDevice.h"

class Dx12RenderDevice final : public IRenderDevice
{
public:
    RendererBackend Backend() const override { return RendererBackend::DirectX12; }

    bool Initialize(HWND hwnd, UINT width, UINT height) override;
    void Shutdown() override;
    bool Resize(UINT width, UINT height) override;
    void PreRender(const float clearColor[4]) override;
    void Render() override;

    ID3D12CommandQueue* GetDx12CommandQueue() override;

private:
    DirectXDevice device_;
};
