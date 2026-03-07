#pragma once

#include "PipelineLibrary.h"

#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl/client.h>

#include <vector>

class Material final
{
public:
    struct MaterialDesc
    {
        PipelineLibrary::PipelineKey pipelineKey;
        std::vector<D3D12_INPUT_ELEMENT_DESC> inputElements;
        ID3D12Resource* textureResource = nullptr;
        DXGI_FORMAT textureFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
        UINT textureMipLevels = 1;
    };

    HRESULT Initialize(
        ID3D12Device* device,
        PipelineLibrary& pipelineLibrary,
        const MaterialDesc& desc);

    void Bind(ID3D12GraphicsCommandList* commandList) const;

private:
    std::shared_ptr<const PipelineLibrary::Pipeline> pipeline_;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> textureHeap_;
};
