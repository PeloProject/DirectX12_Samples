#include "pch.h"
#include "Dx12RenderDevice.h"
#include "DescriptorHeapManager.h"

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

HRESULT Dx12RenderDevice::GetDeviceRemovedReason()
{
    if (s_activeInstance_ == nullptr || s_activeInstance_->device_ == nullptr)
    {
        return E_POINTER;
    }
    return s_activeInstance_->device_->GetDeviceRemovedReason();
}

#ifdef _DEBUG
void Dx12RenderDevice::EnableDebugLayer()
{
    const bool enableDebugLayer = []() -> bool
    {
        char* value = nullptr;
        size_t len = 0;
        const errno_t err = _dupenv_s(&value, &len, "DX12_DEBUG_LAYER");
        const bool enabled = (err == 0 && value != nullptr && strcmp(value, "1") == 0);
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

    renderTargets_.clear();
    primaryHwnd_ = nullptr;
    commandList_.Reset();
    commandAllocator_.Reset();
    commandQueue_.Reset();
    fence_.Reset();
    adapter_.Reset();

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

void Dx12RenderDevice::DumpInfoQueueMessages(const char* context)
{
    if (device_ == nullptr)
    {
        return;
    }

    ComPtr<ID3D12InfoQueue> infoQueue;
    if (FAILED(device_->QueryInterface(IID_PPV_ARGS(&infoQueue))))
    {
        return;
    }

    const UINT64 messageCount = infoQueue->GetNumStoredMessagesAllowedByRetrievalFilter();
    for (UINT64 index = 0; index < messageCount; ++index)
    {
        SIZE_T messageLength = 0;
        if (FAILED(infoQueue->GetMessage(index, nullptr, &messageLength)) || messageLength == 0)
        {
            continue;
        }

        std::vector<unsigned char> storage(messageLength);
        D3D12_MESSAGE* message = reinterpret_cast<D3D12_MESSAGE*>(storage.data());
        if (FAILED(infoQueue->GetMessage(index, message, &messageLength)))
        {
            continue;
        }

        LOG_DEBUG("D3D12InfoQueue[%s] severity=%u id=%d text=%s",
            context != nullptr ? context : "unknown",
            static_cast<unsigned int>(message->Severity),
            static_cast<int>(message->ID),
            message->pDescription != nullptr ? message->pDescription : "(null)");
    }

    if (messageCount > 0)
    {
        infoQueue->ClearStoredMessages();
    }
}
#endif

bool Dx12RenderDevice::Initialize(HWND hwnd, UINT width, UINT height)
{
    isShutdown_ = false;
    s_activeInstance_ = this;
    primaryHwnd_ = hwnd;

#ifdef _DEBUG
    EnableDebugLayer();
#endif

    if (!CreateGraphicsInterface()) return false;
    if (!CreateDevice()) return false;
    if (!CreateCommandList()) return false;
    if (!CreateCommandQueue()) return false;
    SwapChainRenderTarget primaryTarget = {};
    primaryTarget.hwnd = hwnd;
    if (!CreateSwapChain(primaryTarget, hwnd, width, height)) return false;
    if (!CreateRenderTargetView(primaryTarget)) return false;
    renderTargets_[hwnd] = std::move(primaryTarget);
    if (!CreateFence()) return false;

	if (!DescriptorHeapManager::Get().InitializeGlobalTextureHeap(device_.Get()))
    {
        return false;
    }
    return true;
}

bool Dx12RenderDevice::CreateGraphicsInterface()
{
    return SUCCEEDED(CreateDXGIFactory2(0, IID_PPV_ARGS(&dxgiFactory_)));
}

IDXGIAdapter* Dx12RenderDevice::GetAdapter()
{
    if (adapter_ != nullptr)
    {
        return adapter_.Get();
    }

    auto trySelectAdapter = [this](IDXGIAdapter1* candidate, bool allowSoftware) -> bool
    {
        if (candidate == nullptr)
        {
            return false;
        }

        DXGI_ADAPTER_DESC1 desc = {};
        if (FAILED(candidate->GetDesc1(&desc)))
        {
            return false;
        }
        if (!allowSoftware && (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) != 0)
        {
            return false;
        }
        if (FAILED(D3D12CreateDevice(candidate, D3D_FEATURE_LEVEL_11_0, __uuidof(ID3D12Device), nullptr)))
        {
            return false;
        }

        adapter_ = candidate;
        return true;
    };

    const bool forceWarp = []() -> bool
    {
        char* value = nullptr;
        size_t len = 0;
        const errno_t err = _dupenv_s(&value, &len, "DX12_FORCE_WARP");
        const bool enabled = (err == 0 && value != nullptr && strcmp(value, "1") == 0);
        if (value != nullptr)
        {
            free(value);
        }
        return enabled;
    }();

    if (forceWarp)
    {
        ComPtr<IDXGIAdapter> warpAdapter;
        if (SUCCEEDED(dxgiFactory_->EnumWarpAdapter(IID_PPV_ARGS(&warpAdapter))))
        {
            ComPtr<IDXGIAdapter1> warpAdapter1;
            if (SUCCEEDED(warpAdapter.As(&warpAdapter1)) && trySelectAdapter(warpAdapter1.Get(), true))
            {
                return adapter_.Get();
            }
        }
    }

    ComPtr<IDXGIFactory6> factory6;
    if (SUCCEEDED(dxgiFactory_.As(&factory6)))
    {
        for (UINT i = 0;; ++i)
        {
            ComPtr<IDXGIAdapter1> candidate;
            if (factory6->EnumAdapterByGpuPreference(i, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_PPV_ARGS(&candidate)) == DXGI_ERROR_NOT_FOUND)
            {
                break;
            }
            if (trySelectAdapter(candidate.Get(), false))
            {
                return adapter_.Get();
            }
        }
    }

    for (UINT i = 0;; ++i)
    {
        ComPtr<IDXGIAdapter1> candidate;
        if (dxgiFactory_->EnumAdapters1(i, &candidate) == DXGI_ERROR_NOT_FOUND)
        {
            break;
        }
        if (trySelectAdapter(candidate.Get(), false))
        {
            return adapter_.Get();
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

bool Dx12RenderDevice::CreateSwapChain(SwapChainRenderTarget& target, HWND hwnd, UINT width, UINT height)
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
    swapChainDesc.Flags = 0;

    target.hwnd = hwnd;

    ComPtr<IDXGISwapChain1> swapChain;
    const HRESULT hr = dxgiFactory_->CreateSwapChainForHwnd(
        commandQueue_.Get(),
        hwnd,
        &swapChainDesc,
        nullptr,
        nullptr,
        swapChain.GetAddressOf());
    if (FAILED(hr))
    {
        return false;
    }

    return SUCCEEDED(swapChain.As(&target.swapChain));
}

bool Dx12RenderDevice::CreateRenderTargetView(SwapChainRenderTarget& target)
{
    D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
    rtvHeapDesc.NumDescriptors = 2;
    rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    rtvHeapDesc.NodeMask = 0;

    if (FAILED(device_->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&target.rtvHeap))))
    {
        return false;
    }

    DXGI_SWAP_CHAIN_DESC swapChainDesc = {};
    if (target.swapChain == nullptr || FAILED(target.swapChain->GetDesc(&swapChainDesc)))
    {
        return false;
    }

    target.backBuffers.resize(swapChainDesc.BufferCount);
    for (UINT i = 0; i < swapChainDesc.BufferCount; ++i)
    {
        if (FAILED(target.swapChain->GetBuffer(i, IID_PPV_ARGS(&target.backBuffers[i]))))
        {
            return false;
        }
    }

    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = target.rtvHeap->GetCPUDescriptorHandleForHeapStart();
    for (UINT i = 0; i < swapChainDesc.BufferCount; ++i)
    {
        device_->CreateRenderTargetView(target.backBuffers[i].Get(), nullptr, rtvHandle);
        rtvHandle.ptr += device_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    }

    return true;
}

bool Dx12RenderDevice::Resize(UINT width, UINT height)
{
    SwapChainRenderTarget* target = FindRenderTarget(primaryHwnd_);
    if (target == nullptr)
    {
        return false;
    }

    return ResizeRenderTarget(*target, width, height);
}

bool Dx12RenderDevice::ResizeRenderTarget(HWND hwnd, UINT width, UINT height)
{
    SwapChainRenderTarget* target = FindRenderTarget(hwnd);
    if (target == nullptr)
    {
        return false;
    }

    return ResizeRenderTarget(*target, width, height);
}

bool Dx12RenderDevice::ResizeRenderTarget(SwapChainRenderTarget& target, UINT width, UINT height)
{
    if (target.swapChain == nullptr || width == 0 || height == 0)
    {
        return false;
    }

    WaitForPreviousFrame();

    for (auto& buffer : target.backBuffers)
    {
        buffer.Reset();
    }
    target.backBuffers.clear();
    target.rtvHeap.Reset();
	//DescriptorHeapManager::Get().ResetGlobalTextureHeap();

    const HRESULT hr = target.swapChain->ResizeBuffers(0, width, height, DXGI_FORMAT_UNKNOWN, 0);
    if (FAILED(hr))
    {
        return false;
    }

	//DescriptorHeapManager::Get().InitializeGlobalTextureHeap(device_.Get());
    target.barrierDesc.Transition.pResource = nullptr;
    return CreateRenderTargetView(target);
}

bool Dx12RenderDevice::CreateFence()
{
    return SUCCEEDED(device_->CreateFence(fenceValue_, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence_)));
}

void Dx12RenderDevice::PreRender(const float clearColor[4])
{
    PreRenderTarget(primaryHwnd_, clearColor);
}

void Dx12RenderDevice::PreRenderTarget(HWND hwnd, const float clearColor[4])
{
    SwapChainRenderTarget* target = FindRenderTarget(hwnd);
    if (target == nullptr)
    {
        return;
    }

    PreRenderTarget(*target, clearColor);
}

void Dx12RenderDevice::PreRenderTarget(SwapChainRenderTarget& target, const float clearColor[4])
{
    if (target.swapChain == nullptr || target.backBuffers.empty())
    {
        return;
    }

    const auto bbidx = target.swapChain->GetCurrentBackBufferIndex();

    target.barrierDesc.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    target.barrierDesc.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    target.barrierDesc.Transition.pResource = target.backBuffers[bbidx].Get();
    target.barrierDesc.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    target.barrierDesc.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
    target.barrierDesc.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
    commandList_->ResourceBarrier(1, &target.barrierDesc);

    auto rtvH = target.rtvHeap->GetCPUDescriptorHandleForHeapStart();
    rtvH.ptr += bbidx * device_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    commandList_->OMSetRenderTargets(1, &rtvH, true, nullptr);

    const float defaultClearColor[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
    const float* finalClearColor = (clearColor != nullptr) ? clearColor : defaultClearColor;
    commandList_->ClearRenderTargetView(rtvH, finalClearColor, 0, nullptr);
}

void Dx12RenderDevice::Render()
{
    RenderTarget(primaryHwnd_);
}

void Dx12RenderDevice::RenderTarget(HWND hwnd)
{
    SwapChainRenderTarget* target = FindRenderTarget(hwnd);
    if (target == nullptr)
    {
        return;
    }

    RenderTarget(*target);
}

void Dx12RenderDevice::RenderTarget(SwapChainRenderTarget& target)
{
    if (target.swapChain == nullptr || target.backBuffers.empty())
    {
        return;
    }

    target.barrierDesc.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
    target.barrierDesc.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
    commandList_->ResourceBarrier(1, &target.barrierDesc);

    commandList_->Close();

    ID3D12CommandList* commandLists[] = { commandList_.Get() };
    commandQueue_->ExecuteCommandLists(_countof(commandLists), commandLists);

    const HRESULT presentHr = target.swapChain->Present(1, 0);

#ifdef _DEBUG
    if (FAILED(presentHr))
    {
        DumpInfoQueueMessages("Present");
    }
#endif

    WaitForPreviousFrame();

    commandAllocator_->Reset();
    commandList_->Reset(commandAllocator_.Get(), nullptr);

    if (FAILED(presentHr))
    {
        LOG_DEBUG("Present failed. hr=0x%08X", static_cast<unsigned int>(presentHr));
    }
}

bool Dx12RenderDevice::CreateAdditionalRenderTarget(HWND hwnd, UINT width, UINT height)
{
    if (hwnd == nullptr || renderTargets_.find(hwnd) != renderTargets_.end())
    {
        return false;
    }

    SwapChainRenderTarget target = {};
    target.hwnd = hwnd;
    if (!CreateSwapChain(target, hwnd, width, height))
    {
        return false;
    }
    if (!CreateRenderTargetView(target))
    {
        return false;
    }

    renderTargets_[hwnd] = std::move(target);
    return true;
}

void Dx12RenderDevice::DestroyAdditionalRenderTarget(HWND hwnd)
{
    if (hwnd == nullptr || hwnd == primaryHwnd_)
    {
        return;
    }

    WaitForPreviousFrame();
    renderTargets_.erase(hwnd);
}

Dx12RenderDevice::SwapChainRenderTarget* Dx12RenderDevice::FindRenderTarget(HWND hwnd)
{
    const auto it = renderTargets_.find(hwnd);
    if (it == renderTargets_.end())
    {
        return nullptr;
    }
    return &it->second;
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
