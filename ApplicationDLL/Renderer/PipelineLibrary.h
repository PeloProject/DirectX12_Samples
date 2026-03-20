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

    struct PipelineDesc
    {
		ShaderCache::ShaderProgramDesc vertexShader;
        ShaderCache::ShaderProgramDesc pixelShader;
       
        DXGI_FORMAT renderTargetFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
        D3D12_CULL_MODE cullMode = D3D12_CULL_MODE_NONE;
        D3D12_PRIMITIVE_TOPOLOGY_TYPE topologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        bool enableDepth = false;
        bool enableBlend = false;
        std::vector<InputElementDesc> inputElements;
        RootSignatureCache::RootSignatureDesc rootSignatureDesc;
        bool operator==(const PipelineDesc& other) const;
    };

    struct Pipeline
    {
        Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState;
        Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature;
    };

    HRESULT GetOrCreate(
        ID3D12Device* device,
        const PipelineDesc& desc,
        std::shared_ptr<const Pipeline>* outPipeline);

    void Clear();

private:
    struct PipelineDescHasher
    {
        size_t operator()(const PipelineDesc& desc) const;
    };


    /// <summary>
    /// 指定したデバイスと記述に基づいてパイプラインを作成し、結果を出力パラメータに格納する。メソッドはオブジェクトの状態を変更しない（const）。
    /// </summary>
    /// <param name="device">パイプライン作成に使用するID3D12Deviceへのポインタ。</param>
    /// <param name="desc">作成するパイプラインの設定を保持するPipelineDescへの参照。</param>
    /// <param name="outPipeline">作成されたパイプラインを受け取る出力パラメータ。成功時にstd::shared_ptr<const Pipeline>が格納される。</param>
    /// <returns>HRESULTで操作結果を示す。成功時は通常S_OKが返り、失敗時は適切なエラーコードが返される。</returns>
    HRESULT CreatePipeline(
        ID3D12Device* device,
        const PipelineDesc& desc,
        std::shared_ptr<const Pipeline>* outPipeline) const;

	/// <summary>
	/// キャッシュの統計情報を出力します。
	/// </summary>
	void DumpCacheStats() const;

private:
    UINT GetShaderCompileFlags() const;
	std::string DescribePipelineDesc(const PipelineDesc& desc) const;

    mutable std::mutex mutex_;
    std::unordered_map<PipelineDesc, std::shared_ptr<const Pipeline>, PipelineDescHasher> cache_;

	int m_TotalRequestCount = 0;
	int m_CacheHitCount = 0;
	int m_CacheMissCount = 0;
	int m_CreateFailureCount = 0;

};
