#pragma once

#include "RendererBackend.h"

#include <Windows.h>

struct ID3D12CommandQueue;

class IRenderDevice
{
public:
    virtual ~IRenderDevice() = default;

    virtual RendererBackend Backend() const = 0;
    virtual bool Initialize(HWND hwnd, UINT width, UINT height) = 0;
    virtual void Shutdown() = 0;
    virtual bool Resize(UINT width, UINT height) = 0;
    virtual void PreRender(const float clearColor[4]) = 0;
    virtual void Render() = 0;

    virtual ID3D12CommandQueue* GetDx12CommandQueue() { return nullptr; }
};
