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

class PipelineLibrary final
{
public:
    enum class RootParameterType : unsigned int
    {
        DescriptorTableSrv,
        ConstantBufferView
    };

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

    struct RootParameterDesc
    {
        RootParameterType type = RootParameterType::DescriptorTableSrv;
        D3D12_SHADER_VISIBILITY shaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

        // Descriptor-table SRV parameters.
        UINT numDescriptors = 1;
        UINT baseShaderRegister = 0;
        UINT registerSpace = 0;

        // CBV parameters.
        UINT cbvShaderRegister = 0;
        UINT cbvRegisterSpace = 0;

        bool operator==(const RootParameterDesc& other) const;
    };

    struct StaticSamplerDesc
    {
        D3D12_FILTER filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
        D3D12_TEXTURE_ADDRESS_MODE addressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        D3D12_TEXTURE_ADDRESS_MODE addressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        D3D12_TEXTURE_ADDRESS_MODE addressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        UINT shaderRegister = 0;
        UINT registerSpace = 0;
        D3D12_SHADER_VISIBILITY shaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
        D3D12_COMPARISON_FUNC comparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
        D3D12_STATIC_BORDER_COLOR borderColor = D3D12_STATIC_BORDER_COLOR_OPAQUE_WHITE;
        FLOAT mipLODBias = 0.0f;
        UINT maxAnisotropy = 1;
        FLOAT minLOD = 0.0f;
        FLOAT maxLOD = D3D12_FLOAT32_MAX;

        bool operator==(const StaticSamplerDesc& other) const;
    };

    struct PipelineDesc
    {
        std::wstring vertexShaderFile;
        std::string vertexEntryPoint;
        std::string vertexShaderModel;
        std::wstring pixelShaderFile;
        std::string pixelEntryPoint;
        std::string pixelShaderModel;
        DXGI_FORMAT renderTargetFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
        D3D12_CULL_MODE cullMode = D3D12_CULL_MODE_NONE;
        D3D12_PRIMITIVE_TOPOLOGY_TYPE topologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        bool enableDepth = false;
        bool enableBlend = false;
        std::vector<InputElementDesc> inputElements;
        std::vector<RootParameterDesc> rootParameters;
        std::vector<StaticSamplerDesc> staticSamplers;

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
    mutable std::mutex mutex_;
    std::unordered_map<PipelineDesc, std::shared_ptr<const Pipeline>, PipelineDescHasher> cache_;

	int m_TotalRequestCount = 0;
	int m_CacheHitCount = 0;
	int m_CacheMissCount = 0;
	int m_CreateFailureCount = 0;

};
