#pragma once

#include "IRenderDevice.h"

#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl/client.h>
#include <vector>

class Dx12RenderDevice final : public IRenderDevice
{
public:
    Dx12RenderDevice() = default;
    ~Dx12RenderDevice() override;

    RendererBackend Backend() const override { return RendererBackend::DirectX12; }

    bool Initialize(HWND hwnd, UINT width, UINT height) override;
    void Shutdown() override;
    bool Resize(UINT width, UINT height) override;
    void PreRender(const float clearColor[4]) override;
    void Render() override;

    ID3D12CommandQueue* GetDx12CommandQueue() override;

    static ID3D12Device* GetDevice();
    static Microsoft::WRL::ComPtr<ID3D12Device> GetDeviceComPtr();
    static ID3D12GraphicsCommandList* GetCommandList();

private:
    bool CreateGraphicsInterface();
    IDXGIAdapter* GetAdapter();
    bool CreateDevice();
    bool CreateCommandList();
    bool CreateCommandQueue();
    bool CreateSwapChain(HWND hwnd, UINT width, UINT height);
    bool CreateRenderTargetView();
    bool CreateFence();
    void WaitForPreviousFrame();
    void EnableDebugLayer();

#ifdef _DEBUG
    void ReportLiveDeviceObjects(Microsoft::WRL::ComPtr<ID3D12DebugDevice>& debugDevice);
#endif

private:
    static Dx12RenderDevice* s_activeInstance_;

    Microsoft::WRL::ComPtr<ID3D12Device> device_;
    Microsoft::WRL::ComPtr<IDXGIFactory4> dxgiFactory_;
    Microsoft::WRL::ComPtr<IDXGISwapChain4> swapChain_;
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commandList_;
    Microsoft::WRL::ComPtr<ID3D12CommandAllocator> commandAllocator_;
    Microsoft::WRL::ComPtr<ID3D12CommandQueue> commandQueue_;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> rtvHeap_;
    Microsoft::WRL::ComPtr<ID3D12Fence> fence_;
    Microsoft::WRL::ComPtr<IDXGIAdapter> adapter_;
    std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>> backBuffers_;

    D3D12_RESOURCE_BARRIER barrierDesc_ = {};
    UINT64 fenceValue_ = 0;
    bool isShutdown_ = false;
};