#include "pch.h"
#include "DescriptorHeapManager.h"
#include "Material.h"
#include "DX12Texture.h"

/// <summary>
/// ディスクリプタテーブルを使用して単純なテクスチャ付きクアッドを描画するための組み込みマテリアルの説明を作成します。
/// </summary>
/// <param name="textureResource"></param>
/// <returns></returns>
Material::MaterialDesc Material::CreateBuiltInTexturedQuadDesc()
{
    MaterialDesc desc = {};
    desc.pipelineDesc.vertexShader.m_ShaderFile = L"BasicVertexShader.hlsl";
    desc.pipelineDesc.vertexShader.m_EntryPoint = "BasicVS";
    desc.pipelineDesc.vertexShader.m_ShaderModel = "vs_5_0";
    desc.pipelineDesc.pixelShader.m_ShaderFile = L"BasicPixelShader.hlsl";
    desc.pipelineDesc.pixelShader.m_EntryPoint = "BasicPS";
    desc.pipelineDesc.pixelShader.m_ShaderModel = "ps_5_0";
    desc.pipelineDesc.renderTargetFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.pipelineDesc.cullMode = D3D12_CULL_MODE_NONE;
    desc.pipelineDesc.topologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    desc.pipelineDesc.enableDepth = false;
    desc.pipelineDesc.enableBlend = false;
    desc.pipelineDesc.inputElements = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
    };
    desc.pipelineDesc.rootSignatureDesc = {
        
        D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT,
        {
            {
                RootSignatureCache::RootParameterType::DescriptorTableSrv,
                D3D12_SHADER_VISIBILITY_PIXEL,
                1,
                0,
                0,
                0,
                0
            }
        },
        {
            {
                D3D12_FILTER_MIN_MAG_MIP_POINT,
                D3D12_TEXTURE_ADDRESS_MODE_WRAP,
                D3D12_TEXTURE_ADDRESS_MODE_WRAP,
                D3D12_TEXTURE_ADDRESS_MODE_WRAP,
                0,
                0,
                D3D12_SHADER_VISIBILITY_PIXEL,
                D3D12_COMPARISON_FUNC_NEVER,
                D3D12_STATIC_BORDER_COLOR_OPAQUE_WHITE,
                0.0f,
                1,
                0.0f,
                D3D12_FLOAT32_MAX
            }
        }
        
    };
    //desc.parameterBlock.textureBindings = {
    //    { 0, textureResource }
    //};
    return desc;
}

/// <summary>
/// 初期から化します。
/// </summary>
/// <param name="device"></param>
/// <param name="pipelineLibrary"></param>
/// <param name="desc"></param>
/// <returns></returns>
HRESULT Material::Initialize(ID3D12Device* device, PipelineLibrary& pipelineLibrary, const MaterialDesc& desc)
{
    if (device == nullptr)
    {
        return E_INVALIDARG;
    }

    HRESULT hr = pipelineLibrary.GetOrCreateGraphics(device, desc.pipelineDesc, &pipeline_);
    if (FAILED(hr))
    {
        return hr;
    }

    parameterBlock_ = desc.parameterBlock;

    return S_OK;
}

/// <summary>
/// マテリアルを指定したコマンドリストにバインドします。
/// commandList および内部の pipeline_ が有効な場合に、パイプラインステートとルートシグネチャを設定し、
/// グローバルなテクスチャ用ディスクリプタヒープと SRV ディスクリプタテーブルをコマンドリストにセットします。
/// commandList または pipeline_ が nullptr の場合は何もしません。
/// </summary>
/// <param name="commandList">レンダリングコマンドを記録するための
/// ID3D12GraphicsCommandList へのポインタ。nullptr の場合、
/// このメソッドは即座にリターンして何も行いません。
/// </param>
void Material::Bind(ID3D12GraphicsCommandList* commandList) const
{

    if (commandList == nullptr || pipeline_ == nullptr)
    {
        return;
    }

	// パイプラインステートをコマンドリストにセットします。
    commandList->SetPipelineState(pipeline_->pipelineState.Get());

	// ルートシグネチャをコマンドリストにセットします。
    commandList->SetGraphicsRootSignature(pipeline_->rootSignature.Get());

	// テクスチャバインディングが存在するかどうかを確認します。
    bool hasTextureBinding = false;
    for (const auto& binding : parameterBlock_.textureBindings)
    {
        if (binding.textureResource != nullptr)
        {
            hasTextureBinding = true;
            break;
        }
    }

	// テクスチャバインディングが存在する場合、グローバルなテクスチャ用ディスクリプタヒープをコマンドリストにセットします。
    if (hasTextureBinding)
    {
        auto textureHeap = DescriptorHeapManager::Get().GetGlobalTextureHeapAddress();
        commandList->SetDescriptorHeaps(1, textureHeap);
    }

	// 各テクスチャバインディングに対して、SRV ディスクリプタテーブルをコマンドリストにセットします。
    for (const auto& binding : parameterBlock_.textureBindings)
    {
        if (binding.textureResource == nullptr)
        {
            continue;
        }
        auto gpuHandleIndex = static_cast<DX12Texture*>(binding.textureResource)->GetDescriptorIndex();
        auto handle = DescriptorHeapManager::Get().GetGPUHandle(gpuHandleIndex);
        commandList->SetGraphicsRootDescriptorTable(binding.rootParameterIndex, handle);
    }

	// 各定数バッファバインディングに対して、定数バッファビューをコマンドリストにセットします。
    for (const auto& binding : parameterBlock_.constantBufferBindings)
    {
        if (binding.gpuVirtualAddress == 0)
        {
            continue;
        }
        commandList->SetGraphicsRootConstantBufferView(binding.rootParameterIndex, binding.gpuVirtualAddress);
    }
}
