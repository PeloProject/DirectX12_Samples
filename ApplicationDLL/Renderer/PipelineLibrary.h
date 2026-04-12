#pragma once

#include <d3d12.h>
#include <d3dcommon.h>
#include <dxgi1_6.h>
#include <wrl/client.h>

#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include "ShaderCache.h"
#include "RootSignatureCache.h"

class PipelineLibrary final
{
public:

    struct InputElementDesc
    {
        std::string semanticName;
        UINT semanticIndex = 0;
        DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;
        UINT inputSlot = 0;
        UINT alignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
        D3D12_INPUT_CLASSIFICATION inputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
        UINT instanceDataStepRate = 0;

        bool operator==(const InputElementDesc& other) const;
    };

    struct GraphicsPipelineDesc
    {
		ShaderCache::ShaderProgramDesc vertexShader;
        ShaderCache::ShaderProgramDesc pixelShader;
       
        DXGI_FORMAT renderTargetFormat = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
        D3D12_CULL_MODE cullMode = D3D12_CULL_MODE_NONE;
        D3D12_PRIMITIVE_TOPOLOGY_TYPE topologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        bool enableDepth = false;
        bool enableBlend = false;
        std::vector<InputElementDesc> inputElements;
        RootSignatureCache::RootSignatureDesc rootSignatureDesc;
        bool operator==(const GraphicsPipelineDesc& other) const;
    };

    struct ComputePipelineDesc
    {
		ShaderCache::ShaderProgramDesc computeShader;
        RootSignatureCache::RootSignatureDesc rootSignatureDesc;
 
    };

    struct GraphicsPipeline
    {
        Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState;
        Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature;
    };

    struct ComputePipeline
    {
        Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState;
        Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature;
    };

    HRESULT GetOrCreateGraphics(
        ID3D12Device* device,
        const GraphicsPipelineDesc& desc,
        std::shared_ptr<const GraphicsPipeline>* outPipeline);

    HRESULT GetOrCreateCompute(
        ID3D12Device* device,
        const ComputePipelineDesc& desc,
        std::shared_ptr<const ComputePipeline>* outPipeline);

    void Clear();

private:
    struct PipelineDescHasher
    {
        size_t operator()(const GraphicsPipelineDesc& desc) const;
    };


    /// <summary>
    /// 指定したデバイスと記述に基づいてパイプラインを作成し、
    /// 結果を出力パラメータに格納する。メソッドはオブジェクトの状態を変更しない（const）。
    /// </summary>
    HRESULT CreateGraphicsPipeline(
        ID3D12Device* device,
        const GraphicsPipelineDesc& desc,
        std::shared_ptr<const GraphicsPipeline>* outPipeline) const;

	/// <summary>
	/// キャッシュの統計情報を出力します。
	/// </summary>
	void DumpCacheStats() const;

private:

	void DescribePipelineDesc(const GraphicsPipelineDesc& desc) const;

    mutable std::mutex mutex_;
    std::unordered_map<GraphicsPipelineDesc, std::shared_ptr<const GraphicsPipeline>, PipelineDescHasher> m_GraphicsCache;

	int m_TotalRequestCount = 0;
	int m_CacheHitCount = 0;
	int m_CacheMissCount = 0;
	int m_CreateFailureCount = 0;

};
