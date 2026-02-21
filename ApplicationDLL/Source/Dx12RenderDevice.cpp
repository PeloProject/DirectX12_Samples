#include "pch.h"
#include "Dx12RenderDevice.h"

bool Dx12RenderDevice::Initialize(HWND hwnd, UINT width, UINT height)
{
    return device_.Initialize(hwnd, width, height);
}

void Dx12RenderDevice::Shutdown()
{
    device_.Shutdown();
}

bool Dx12RenderDevice::Resize(UINT width, UINT height)
{
    return device_.Resize(width, height);
}

void Dx12RenderDevice::PreRender(const float clearColor[4])
{
    device_.PreRender(clearColor);
}

void Dx12RenderDevice::Render()
{
    device_.Render();
}

ID3D12CommandQueue* Dx12RenderDevice::GetDx12CommandQueue()
{
    return device_.GetCommandQueue();
}
