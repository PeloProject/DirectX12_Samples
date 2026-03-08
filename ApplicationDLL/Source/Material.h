#pragma once

#include "PipelineLibrary.h"
#include "RHITexture.h"
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
        RHITexture* textureResource = nullptr;
    };

    HRESULT Initialize(
        ID3D12Device* device,
        PipelineLibrary& pipelineLibrary,
        const MaterialDesc& desc);

    void Bind(ID3D12GraphicsCommandList* commandList) const;

    void SetTexture(RHITexture* texture)
    {
        m_pTexture = texture;
	}   

private:
    std::shared_ptr<const PipelineLibrary::Pipeline> pipeline_;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> textureHeap_;

	RHITexture* m_pTexture = nullptr;
};
