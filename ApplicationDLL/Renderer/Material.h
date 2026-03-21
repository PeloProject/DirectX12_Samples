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
    struct MaterialParameterBlock
    {
        struct TextureBinding
        {
            UINT rootParameterIndex = 0;
            RHITexture* textureResource = nullptr;
        };

        struct ConstantBufferBinding
        {
            UINT rootParameterIndex = 0;
            D3D12_GPU_VIRTUAL_ADDRESS gpuVirtualAddress = 0;
        };

        std::vector<TextureBinding> textureBindings;
        std::vector<ConstantBufferBinding> constantBufferBindings;
    };

    struct MaterialDesc
    {
        PipelineLibrary::GraphicsPipelineDesc pipelineDesc;
        MaterialParameterBlock parameterBlock;
    };

    static MaterialDesc CreateBuiltInTexturedQuadDesc(RHITexture* textureResource);

    HRESULT Initialize(
        ID3D12Device* device,
        PipelineLibrary& pipelineLibrary,
        const MaterialDesc& desc);

    void Bind(ID3D12GraphicsCommandList* commandList) const;

    void SetTexture(RHITexture* texture)
    {
        if (parameterBlock_.textureBindings.empty())
        {
            parameterBlock_.textureBindings.push_back({ 0, texture });
            return;
        }
        parameterBlock_.textureBindings[0].textureResource = texture;
	}   

private:
    std::shared_ptr<const PipelineLibrary::Pipeline> pipeline_;
    MaterialParameterBlock parameterBlock_;
};
