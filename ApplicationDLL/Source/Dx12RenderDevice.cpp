#include "pch.h"
#include "Dx12RenderDevice.h"

#include <Windows.h>
#include <cstring>

#ifdef _DEBUG
#include <dxgidebug.h>
#pragma comment(lib, "dxguid.lib")
#endif

using Microsoft::WRL::ComPtr;

Dx12RenderDevice* Dx12RenderDevice::s_activeInstance_ = nullptr;

Dx12RenderDevice::~Dx12RenderDevice()
{
    if (!isShutdown_)
    {
        Shutdown();
    }
}

ID3D12Device* Dx12RenderDevice::GetDevice()
{
    return (s_activeInstance_ != nullptr) ? s_activeInstance_->device_.Get() : nullptr;
}

ComPtr<ID3D12Device> Dx12RenderDevice::GetDeviceComPtr()
{
    return (s_activeInstance_ != nullptr) ? s_activeInstance_->device_ : ComPtr<ID3D12Device>();
}

ID3D12GraphicsCommandList* Dx12RenderDevice::GetCommandList()
{
    return (s_activeInstance_ != nullptr) ? s_activeInstance_->commandList_.Get() : nullptr;
}

#ifdef _DEBUG
void Dx12RenderDevice::EnableDebugLayer()
{
    const bool enableDebugLayer = []() -> bool
    {
        char* value = nullptr;
        size_t len = 0;
        const errno_t err = _dupenv_s(&value, &len, "DX12_DEBUG_LAYER");
        const bool enabled = !(err == 0 && value != nullptr && strcmp(value, "0") == 0);
        if (value != nullptr)
        {
            free(value);
        }
        return enabled;
    }();

    if (!enableDebugLayer)
    {
        return;
    }

    const bool enableGpuValidation = []() -> bool
    {
        char* value = nullptr;
        size_t len = 0;
        const errno_t err = _dupenv_s(&value, &len, "DX12_GPU_VALIDATION");
        const bool enabled = (err == 0 && value != nullptr && strcmp(value, "1") == 0);
        if (value != nullptr)
        {
            free(value);
        }
        return enabled;
    }();

    ComPtr<ID3D12Debug> debugController;
    if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController))))
    {
        debugController->EnableDebugLayer();

        ComPtr<ID3D12Debug1> debugController1;
        if (SUCCEEDED(debugController.As(&debugController1)))
        {
            debugController1->SetEnableGPUBasedValidation(enableGpuValidation ? TRUE : FALSE);
            debugController1->SetEnableSynchronizedCommandQueueValidation(enableGpuValidation ? TRUE : FALSE);
        }

        ComPtr<ID3D12Debug3> debugController3;
        if (SUCCEEDED(debugController.As(&debugController3)))
        {
            debugController3->SetEnableGPUBasedValidation(enableGpuValidation ? TRUE : FALSE);
        }
    }

    ComPtr<IDXGIInfoQueue> dxgiInfoQueue;
    if (SUCCEEDED(DXGIGetDebugInterface1(0, IID_PPV_ARGS(&dxgiInfoQueue))))
    {
        dxgiInfoQueue->SetBreakOnSeverity(DXGI_DEBUG_ALL, DXGI_INFO_QUEUE_MESSAGE_SEVERITY_ERROR, true);
        dxgiInfoQueue->SetBreakOnSeverity(DXGI_DEBUG_ALL, DXGI_INFO_QUEUE_MESSAGE_SEVERITY_CORRUPTION, true);
    }
}
#else
void Dx12RenderDevice::EnableDebugLayer() {}
#endif

void Dx12RenderDevice::Shutdown()
{
    if (isShutdown_)
    {
        return;
    }
    isShutdown_ = true;

    WaitForPreviousFrame();

    swapChain_.Reset();
    commandList_.Reset();
    commandAllocator_.Reset();
    commandQueue_.Reset();
    rtvHeap_.Reset();
    fence_.Reset();
    adapter_.Reset();

    for (auto& buffer : backBuffers_)
    {
        buffer.Reset();
    }
    backBuffers_.clear();

#ifdef _DEBUG
    ComPtr<ID3D12DebugDevice> debugDevice;
    if (device_)
    {
        device_.As(&debugDevice);
    }
#endif

    device_.Reset();
    dxgiFactory_.Reset();

#ifdef _DEBUG
    if (debugDevice)
    {
        ReportLiveDeviceObjects(debugDevice);
    }
#endif

    if (s_activeInstance_ == this)
    {
        s_activeInstance_ = nullptr;
    }
}

#ifdef _DEBUG
void Dx12RenderDevice::ReportLiveDeviceObjects(ComPtr<ID3D12DebugDevice>& debugDevice)
{
    if (!debugDevice)
    {
        return;
    }

    debugDevice->ReportLiveDeviceObjects(
        D3D12_RLDO_SUMMARY |
        D3D12_RLDO_DETAIL |
        D3D12_RLDO_IGNORE_INTERNAL);

    debugDevice.Reset();

    ComPtr<IDXGIDebug1> dxgiDebug;
    if (SUCCEEDED(DXGIGetDebugInterface1(0, IID_PPV_ARGS(&dxgiDebug))))
    {
        dxgiDebug->ReportLiveObjects(
            DXGI_DEBUG_ALL,
            DXGI_DEBUG_RLO_FLAGS(DXGI_DEBUG_RLO_DETAIL | DXGI_DEBUG_RLO_IGNORE_INTERNAL));
    }
}
#endif

bool Dx12RenderDevice::Initialize(HWND hwnd, UINT width, UINT height)
{
    isShutdown_ = false;
    s_activeInstance_ = this;

#ifdef _DEBUG
    EnableDebugLayer();
#endif

    if (!CreateGraphicsInterface()) return false;
    if (!CreateDevice()) return false;
    if (!CreateCommandList()) return false;
    if (!CreateCommandQueue()) return false;
    if (!CreateSwapChain(hwnd, width, height)) return false;
    if (!CreateRenderTargetView()) return false;
    if (!CreateFence()) return false;
    return true;
}

bool Dx12RenderDevice::CreateGraphicsInterface()
{
    return SUCCEEDED(CreateDXGIFactory2(0, IID_PPV_ARGS(&dxgiFactory_)));
}

IDXGIAdapter* Dx12RenderDevice::GetAdapter()
{
    for (UINT i = 0;; ++i)
    {
        ComPtr<IDXGIAdapter> tempAdapter;
        if (dxgiFactory_->EnumAdapters(i, &tempAdapter) == DXGI_ERROR_NOT_FOUND)
        {
            break;
        }
        DXGI_ADAPTER_DESC desc;
        tempAdapter->GetDesc(&desc);

        if (desc.VendorId == 0x10DE)
        {
            adapter_ = tempAdapter;
            break;
        }
    }

    return adapter_.Get();
}

bool Dx12RenderDevice::CreateDevice()
{
    IDXGIAdapter* adapter = GetAdapter();

    D3D_FEATURE_LEVEL levels[] = {
        D3D_FEATURE_LEVEL_12_1,
        D3D_FEATURE_LEVEL_12_0,
        D3D_FEATURE_LEVEL_11_1,
        D3D_FEATURE_LEVEL_11_0,
    };

    for (auto level : levels)
    {
        if (SUCCEEDED(D3D12CreateDevice(adapter, level, IID_PPV_ARGS(device_.GetAddressOf()))))
        {
            break;
        }
    }

    return device_ != nullptr;
}

bool Dx12RenderDevice::CreateCommandList()
{
    if (FAILED(device_->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(commandAllocator_.GetAddressOf()))))
    {
        return false;
    }

    if (FAILED(device_->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, commandAllocator_.Get(), nullptr, IID_PPV_ARGS(&commandList_))))
    {
        return false;
    }

    return true;
}

bool Dx12RenderDevice::CreateCommandQueue()
{
    D3D12_COMMAND_QUEUE_DESC queueDesc = {};
    queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    queueDesc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
    queueDesc.NodeMask = 0;

    return SUCCEEDED(device_->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&commandQueue_)));
}

bool Dx12RenderDevice::CreateSwapChain(HWND hwnd, UINT width, UINT height)
{
    DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
    swapChainDesc.Width = width;
    swapChainDesc.Height = height;
    swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    swapChainDesc.Stereo = false;
    swapChainDesc.SampleDesc.Count = 1;
    swapChainDesc.SampleDesc.Quality = 0;
    swapChainDesc.BufferUsage = DXGI_USAGE_BACK_BUFFER;
    swapChainDesc.BufferCount = 2;
    swapChainDesc.Scaling = DXGI_SCALING_STRETCH;
    swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    swapChainDesc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
    swapChainDesc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

    return SUCCEEDED(dxgiFactory_->CreateSwapChainForHwnd(
        commandQueue_.Get(),
        hwnd,
        &swapChainDesc,
        nullptr,
        nullptr,
        reinterpret_cast<IDXGISwapChain1**>(swapChain_.GetAddressOf())));
}

bool Dx12RenderDevice::CreateRenderTargetView()
{
    D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
    rtvHeapDesc.NumDescriptors = 2;
    rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    rtvHeapDesc.NodeMask = 0;

    if (FAILED(device_->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&rtvHeap_))))
    {
        return false;
    }

    DXGI_SWAP_CHAIN_DESC swapChainDesc = {};
    if (FAILED(swapChain_->GetDesc(&swapChainDesc)))
    {
        return false;
    }

    backBuffers_.resize(swapChainDesc.BufferCount);
    for (UINT i = 0; i < swapChainDesc.BufferCount; ++i)
    {
        if (FAILED(swapChain_->GetBuffer(i, IID_PPV_ARGS(&backBuffers_[i]))))
        {
            return false;
        }
    }

    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = rtvHeap_->GetCPUDescriptorHandleForHeapStart();
    for (UINT i = 0; i < swapChainDesc.BufferCount; ++i)
    {
        device_->CreateRenderTargetView(backBuffers_[i].Get(), nullptr, rtvHandle);
        rtvHandle.ptr += device_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    }

    return true;
}

bool Dx12RenderDevice::Resize(UINT width, UINT height)
{
    if (swapChain_ == nullptr || width == 0 || height == 0)
    {
        return false;
    }

    WaitForPreviousFrame();

    for (auto& buffer : backBuffers_)
    {
        buffer.Reset();
    }
    backBuffers_.clear();
    rtvHeap_.Reset();

    const HRESULT hr = swapChain_->ResizeBuffers(0, width, height, DXGI_FORMAT_UNKNOWN, DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH);
    if (FAILED(hr))
    {
        return false;
    }

    barrierDesc_.Transition.pResource = nullptr;
    return CreateRenderTargetView();
}

bool Dx12RenderDevice::CreateFence()
{
    return SUCCEEDED(device_->CreateFence(fenceValue_, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence_)));
}

void Dx12RenderDevice::PreRender(const float clearColor[4])
{
    if (swapChain_ == nullptr || backBuffers_.empty())
    {
        return;
    }

    const auto bbidx = swapChain_->GetCurrentBackBufferIndex();

    barrierDesc_.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrierDesc_.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    barrierDesc_.Transition.pResource = backBuffers_[bbidx].Get();
    barrierDesc_.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    barrierDesc_.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
    barrierDesc_.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
    commandList_->ResourceBarrier(1, &barrierDesc_);

    auto rtvH = rtvHeap_->GetCPUDescriptorHandleForHeapStart();
    rtvH.ptr += bbidx * device_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    commandList_->OMSetRenderTargets(1, &rtvH, true, nullptr);

    const float defaultClearColor[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
    const float* finalClearColor = (clearColor != nullptr) ? clearColor : defaultClearColor;
    commandList_->ClearRenderTargetView(rtvH, finalClearColor, 0, nullptr);
}

void Dx12RenderDevice::Render()
{
    if (swapChain_ == nullptr || backBuffers_.empty())
    {
        return;
    }

    barrierDesc_.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
    barrierDesc_.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
    commandList_->ResourceBarrier(1, &barrierDesc_);

    commandList_->Close();

    ID3D12CommandList* commandLists[] = { commandList_.Get() };
    commandQueue_->ExecuteCommandLists(_countof(commandLists), commandLists);

    WaitForPreviousFrame();

    commandAllocator_->Reset();
    commandList_->Reset(commandAllocator_.Get(), nullptr);

    swapChain_->Present(1, 0);
}

void Dx12RenderDevice::WaitForPreviousFrame()
{
    if (commandQueue_ == nullptr || fence_ == nullptr)
    {
        return;
    }

    const HRESULT signalHr = commandQueue_->Signal(fence_.Get(), ++fenceValue_);
    if (FAILED(signalHr))
    {
        return;
    }

    if (fence_->GetCompletedValue() < fenceValue_)
    {
        HANDLE eventHandle = CreateEvent(nullptr, FALSE, FALSE, nullptr);
        if (eventHandle != nullptr)
        {
            const HRESULT setEventHr = fence_->SetEventOnCompletion(fenceValue_, eventHandle);
            if (SUCCEEDED(setEventHr))
            {
                WaitForSingleObject(eventHandle, 1000);
            }
            CloseHandle(eventHandle);
        }
    }
}

ID3D12CommandQueue* Dx12RenderDevice::GetDx12CommandQueue()
{
    return commandQueue_.Get();
}