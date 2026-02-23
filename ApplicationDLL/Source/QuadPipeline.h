#pragma once

#include <d3d12.h>
#include <d3dcommon.h>
#include <wrl/client.h>

class QuadPipeline final
{
public:
    HRESULT Initialize(
        ID3D12Device* device,
        ID3DBlob* vertexShaderBlob,
        ID3DBlob* pixelShaderBlob);

    void Bind(ID3D12GraphicsCommandList* commandList) const;

private:
    Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState_;
    Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_;
};
