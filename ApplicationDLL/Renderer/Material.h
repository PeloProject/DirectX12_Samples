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
    /// <summary>
	/// マテリアルのパラメータブロックは、
    /// マテリアルが使用するテクスチャや定数バッファなどのリソースバインディングを定義します。
    /// </summary>
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

    static MaterialDesc CreateBuiltInTexturedQuadDesc();

    HRESULT Initialize(ID3D12Device* device, PipelineLibrary& pipelineLibrary, const MaterialDesc& desc);

    void Bind(ID3D12GraphicsCommandList* commandList) const;

    /// <summary>
	/// マテリアルのテクスチャーを更新します。
    /// parameterBlock_ の最初のテクスチャバインディングを新しいテクスチャーリソースで上書きします。
    /// </summary>
    /// <param name="texture"></param>
    void SetTexture(RHITexture* texture);

private:
    std::shared_ptr<const PipelineLibrary::GraphicsPipeline> m_pPipeline;
    MaterialParameterBlock m_ParameterBlock;
};
