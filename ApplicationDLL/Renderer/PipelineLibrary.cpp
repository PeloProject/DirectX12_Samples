#include "pch.h"
#include "PipelineLibrary.h"

#include "ShaderCompiler.h"
#include "ShaderCache.h"
#include "RootSignatureCache.h"

#include <cstring>
#include <vector>

namespace
{
inline void HashCombine(size_t& seed, size_t value)
{
    seed ^= value + 0x9e3779b9 + (seed << 6) + (seed >> 2);
}

inline size_t HashFloat(float value)
{
    static_assert(sizeof(float) == sizeof(unsigned int), "Unexpected float size");
    unsigned int bits = 0;
    memcpy(&bits, &value, sizeof(float));
    return std::hash<unsigned int>{}(bits);
}
}

bool PipelineLibrary::InputElementDesc::operator==(const InputElementDesc& other) const
{
    return semanticName == other.semanticName &&
        semanticIndex == other.semanticIndex &&
        format == other.format &&
        inputSlot == other.inputSlot &&
        alignedByteOffset == other.alignedByteOffset &&
        inputSlotClass == other.inputSlotClass &&
        instanceDataStepRate == other.instanceDataStepRate;
}



bool PipelineLibrary::PipelineDesc::operator==(const PipelineDesc& other) const
{
    return vertexShader == other.vertexShader &&
        pixelShader == other.pixelShader &&
        renderTargetFormat == other.renderTargetFormat &&
        cullMode == other.cullMode &&
        topologyType == other.topologyType &&
        enableDepth == other.enableDepth &&
        enableBlend == other.enableBlend &&
        inputElements == other.inputElements &&
        rootSignatureDesc == other.rootSignatureDesc;
}

size_t PipelineLibrary::PipelineDescHasher::operator()(const PipelineDesc& desc) const
{
    size_t seed = 0;
    HashCombine(seed, std::hash<std::wstring>{}(desc.vertexShader.m_ShaderFile));
    HashCombine(seed, std::hash<std::string>{}(desc.vertexShader.m_EntryPoint));
    HashCombine(seed, std::hash<std::string>{}(desc.vertexShader.m_ShaderModel));
    HashCombine(seed, std::hash<std::wstring>{}(desc.pixelShader.m_ShaderFile));
    HashCombine(seed, std::hash<std::string>{}(desc.pixelShader.m_EntryPoint));
    HashCombine(seed, std::hash<std::string>{}(desc.pixelShader.m_ShaderModel));
    HashCombine(seed, std::hash<int>{}(static_cast<int>(desc.renderTargetFormat)));
    HashCombine(seed, std::hash<int>{}(static_cast<int>(desc.cullMode)));
    HashCombine(seed, std::hash<int>{}(static_cast<int>(desc.topologyType)));
    HashCombine(seed, std::hash<bool>{}(desc.enableDepth));
    HashCombine(seed, std::hash<bool>{}(desc.enableBlend));
    for (const auto& element : desc.inputElements)
    {
        HashCombine(seed, std::hash<std::string>{}(element.semanticName));
        HashCombine(seed, std::hash<UINT>{}(element.semanticIndex));
        HashCombine(seed, std::hash<int>{}(static_cast<int>(element.format)));
        HashCombine(seed, std::hash<UINT>{}(element.inputSlot));
        HashCombine(seed, std::hash<UINT>{}(element.alignedByteOffset));
        HashCombine(seed, std::hash<int>{}(static_cast<int>(element.inputSlotClass)));
        HashCombine(seed, std::hash<UINT>{}(element.instanceDataStepRate));
    }
    const auto& root = desc.rootSignatureDesc;
    {
        for (const auto& param : root.rootSignatureParameters)
        {
            HashCombine(seed, std::hash<int>{}(static_cast<int>(param.type)));
            HashCombine(seed, std::hash<int>{}(static_cast<int>(param.shaderVisibility)));
            HashCombine(seed, std::hash<UINT>{}(param.numDescriptors));
            HashCombine(seed, std::hash<UINT>{}(param.baseShaderRegister));
            HashCombine(seed, std::hash<UINT>{}(param.registerSpace));
            HashCombine(seed, std::hash<UINT>{}(param.cbvShaderRegister));
            HashCombine(seed, std::hash<UINT>{}(param.cbvRegisterSpace));
        }
        for (const auto& sampler : root.staticSamplers)
        {
            HashCombine(seed, std::hash<int>{}(static_cast<int>(sampler.filter)));
            HashCombine(seed, std::hash<int>{}(static_cast<int>(sampler.addressU)));
            HashCombine(seed, std::hash<int>{}(static_cast<int>(sampler.addressV)));
            HashCombine(seed, std::hash<int>{}(static_cast<int>(sampler.addressW)));
            HashCombine(seed, std::hash<UINT>{}(sampler.shaderRegister));
            HashCombine(seed, std::hash<UINT>{}(sampler.registerSpace));
            HashCombine(seed, std::hash<int>{}(static_cast<int>(sampler.shaderVisibility)));
            HashCombine(seed, std::hash<int>{}(static_cast<int>(sampler.comparisonFunc)));
            HashCombine(seed, std::hash<int>{}(static_cast<int>(sampler.borderColor)));
            HashCombine(seed, HashFloat(sampler.mipLODBias));
            HashCombine(seed, std::hash<UINT>{}(sampler.maxAnisotropy));
            HashCombine(seed, HashFloat(sampler.minLOD));
            HashCombine(seed, HashFloat(sampler.maxLOD));
        }
    }
    return seed;
}

HRESULT PipelineLibrary::GetOrCreate(
    ID3D12Device* device,
    const PipelineDesc& desc,
    std::shared_ptr<const Pipeline>* outPipeline)
{
	m_TotalRequestCount++;

    if (device == nullptr || outPipeline == nullptr || desc.inputElements.empty())
    {
        DumpCacheStats();
        return E_INVALIDARG;
    }

	// キャッシュからの取得を試みる。見つかった場合は outPipeline に設定して成功を返す。
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = cache_.find(desc);
        if (it != cache_.end())
        {
			m_CacheHitCount++;
            *outPipeline = it->second;
            DumpCacheStats();
            return S_OK;
        }
    }

	// キャッシュに存在しない場合は新規作成を試みる。成功した場合はキャッシュに追加して outPipeline に設定する。
	//m_CacheMissCount++;
    std::shared_ptr<const Pipeline> createdPipeline;
    HRESULT hr = CreatePipeline(device, desc, &createdPipeline);
    if (FAILED(hr))
    {
		m_CreateFailureCount++;
        DumpCacheStats();
        return hr;
    }

    std::lock_guard<std::mutex> lock(mutex_);
    auto [it, inserted] = cache_.emplace(desc, createdPipeline);
    *outPipeline = it->second;
    (void)inserted;
    DumpCacheStats();
    return S_OK;
}

void PipelineLibrary::DumpCacheStats() const
{

    const size_t total = m_TotalRequestCount;
    const size_t hits = m_CacheHitCount;
    const size_t misses = total - hits;
    const size_t failures = m_CreateFailureCount;
    const double hitRate = (total > 0) ? (static_cast<double>(hits) / total) * 100.0 : 0.0;
    OutputDebugStringA("PipelineLibrary Cache Stats:\n");
    OutputDebugStringA(("  Total Requests: " + std::to_string(total) + "\n").c_str());
    OutputDebugStringA(("  Cache Hits: " + std::to_string(hits) + "\n").c_str());
    OutputDebugStringA(("  Cache Misses: " + std::to_string(misses) + "\n").c_str());
    OutputDebugStringA(("  Create Failures: " + std::to_string(failures) + "\n").c_str());
    OutputDebugStringA(("  Hit Rate: " + std::to_string(hitRate) + "%\n").c_str());
}

void PipelineLibrary::Clear()
{
    std::lock_guard<std::mutex> lock(mutex_);
    cache_.clear();
}


///=====================================================
/// <summary>
/// 指定されたデバイスとパイプライン記述に基づいてグラフィックスパイプラインを作成します。シェーダーのコンパイル、ルートシグネチャの生成、パイプラインステートの構築を行い、成功時に outPipeline に作成したパイプラインを設定します。
/// </summary>
/// <param name="device">パイプライン（ルートシグネチャやパイプラインステート）を作成するために使用する ID3D12Device。nullptr ではないことが期待されます。</param>
/// <param name="desc">パイプラインの設定を表す PipelineDesc。頂点/ピクセルシェーダーのファイル名、エントリポイント、シェーダーモデル、ルートパラメータ、スタティックサンプラー、入力要素、ラスタライザ／ブレンド／デプス設定などを含みます。</param>
/// <param name="outPipeline">作成されたパイプラインを受け取る出力パラメータ。成功時に std::shared_ptr<const Pipeline> が格納されます。nullptr ではないことが期待されます。</param>
/// <returns>HRESULT。成功時は S_OK を返します。失敗時はシェーダーコンパイル、ルートシグネチャのシリアライズ、CreateRootSignature やパイプラインステート作成などで発生した HRESULT エラーコードを返します。エラー時にエラーブロブの内容がログ出力される場合があります。</returns>
///=======================================================
HRESULT PipelineLibrary::CreatePipeline(
    ID3D12Device* device,
    const PipelineDesc& desc,
    std::shared_ptr<const Pipeline>* outPipeline) const
{

    const UINT kCompileFlags = GetShaderCompileFlags();

    // 頂点シェーダーのコンパイル
    Microsoft::WRL::ComPtr<ID3DBlob> vertexShaderBlob;
    bool isError = ShaderCache::GetorCreate(
        desc.vertexShader,
        &vertexShaderBlob);

    if (isError || vertexShaderBlob == nullptr)
    {
        return E_FAIL;
    };


	// ピクセルシェーダーのコンパイル
    Microsoft::WRL::ComPtr<ID3DBlob> pixelShaderBlob;
    isError = ShaderCache::GetorCreate(
        desc.pixelShader,
        &pixelShaderBlob);

    if (isError || pixelShaderBlob == nullptr)
    {
        return E_FAIL;
    };

    auto createdPipeline = std::make_shared<Pipeline>();

    RootSignatureCache::GetOrCreate(
        device,
        desc.rootSignatureDesc,
		&createdPipeline->rootSignature);

	// グラフィックスパイプラインステートの構築。D3D12_GRAPHICS_PIPELINE_STATE_DESC を使用してパイプラインステート記述
    D3D12_GRAPHICS_PIPELINE_STATE_DESC pipelineDesc = {};
    pipelineDesc.pRootSignature = createdPipeline->rootSignature.Get();
    pipelineDesc.VS.BytecodeLength = vertexShaderBlob->GetBufferSize();
    pipelineDesc.VS.pShaderBytecode = vertexShaderBlob->GetBufferPointer();
    pipelineDesc.PS.BytecodeLength = pixelShaderBlob->GetBufferSize();
    pipelineDesc.PS.pShaderBytecode = pixelShaderBlob->GetBufferPointer();
    pipelineDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;
    pipelineDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
    pipelineDesc.RasterizerState.CullMode = desc.cullMode;
    pipelineDesc.RasterizerState.FrontCounterClockwise = FALSE;
    pipelineDesc.RasterizerState.DepthBias = D3D12_DEFAULT_DEPTH_BIAS;
    pipelineDesc.RasterizerState.DepthBiasClamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP;
    pipelineDesc.RasterizerState.SlopeScaledDepthBias = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
    pipelineDesc.RasterizerState.DepthClipEnable = TRUE;
    pipelineDesc.RasterizerState.MultisampleEnable = FALSE;
    pipelineDesc.RasterizerState.AntialiasedLineEnable = FALSE;
    pipelineDesc.RasterizerState.ForcedSampleCount = 0;
    pipelineDesc.RasterizerState.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;

    pipelineDesc.BlendState.AlphaToCoverageEnable = FALSE;
    pipelineDesc.BlendState.IndependentBlendEnable = FALSE;

    D3D12_RENDER_TARGET_BLEND_DESC renderTargetBlendDesc = {};
    renderTargetBlendDesc.BlendEnable = desc.enableBlend ? TRUE : FALSE;
    renderTargetBlendDesc.LogicOpEnable = FALSE;
    renderTargetBlendDesc.SrcBlend = D3D12_BLEND_ONE;
    renderTargetBlendDesc.DestBlend = D3D12_BLEND_ZERO;
    renderTargetBlendDesc.BlendOp = D3D12_BLEND_OP_ADD;
    renderTargetBlendDesc.SrcBlendAlpha = D3D12_BLEND_ONE;
    renderTargetBlendDesc.DestBlendAlpha = D3D12_BLEND_ZERO;
    renderTargetBlendDesc.BlendOpAlpha = D3D12_BLEND_OP_ADD;
    renderTargetBlendDesc.LogicOp = D3D12_LOGIC_OP_NOOP;
    renderTargetBlendDesc.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
    pipelineDesc.BlendState.RenderTarget[0] = renderTargetBlendDesc;

    pipelineDesc.DepthStencilState.DepthEnable = desc.enableDepth ? TRUE : FALSE;
    pipelineDesc.DepthStencilState.DepthWriteMask = desc.enableDepth ? D3D12_DEPTH_WRITE_MASK_ALL : D3D12_DEPTH_WRITE_MASK_ZERO;
    pipelineDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
    pipelineDesc.DepthStencilState.StencilEnable = FALSE;
    pipelineDesc.DepthStencilState.StencilReadMask = D3D12_DEFAULT_STENCIL_READ_MASK;
    pipelineDesc.DepthStencilState.StencilWriteMask = D3D12_DEFAULT_STENCIL_WRITE_MASK;
    pipelineDesc.DepthStencilState.FrontFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;
    pipelineDesc.DepthStencilState.FrontFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
    pipelineDesc.DepthStencilState.FrontFace.StencilPassOp = D3D12_STENCIL_OP_KEEP;
    pipelineDesc.DepthStencilState.FrontFace.StencilFunc = D3D12_COMPARISON_FUNC_ALWAYS;
    pipelineDesc.DepthStencilState.BackFace = pipelineDesc.DepthStencilState.FrontFace;

    std::vector<D3D12_INPUT_ELEMENT_DESC> inputLayout(desc.inputElements.size());
    for (size_t i = 0; i < desc.inputElements.size(); ++i)
    {
        const auto& source = desc.inputElements[i];
        D3D12_INPUT_ELEMENT_DESC& target = inputLayout[i];
        target.SemanticName = source.semanticName.c_str();
        target.SemanticIndex = source.semanticIndex;
        target.Format = source.format;
        target.InputSlot = source.inputSlot;
        target.AlignedByteOffset = source.alignedByteOffset;
        target.InputSlotClass = source.inputSlotClass;
        target.InstanceDataStepRate = source.instanceDataStepRate;
    }

    pipelineDesc.InputLayout.pInputElementDescs = inputLayout.data();
    pipelineDesc.InputLayout.NumElements = static_cast<UINT>(inputLayout.size());
    pipelineDesc.IBStripCutValue = D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_DISABLED;
    pipelineDesc.PrimitiveTopologyType = desc.topologyType;
    pipelineDesc.NumRenderTargets = 1;
    pipelineDesc.RTVFormats[0] = desc.renderTargetFormat;
    pipelineDesc.SampleDesc.Count = 1;
    pipelineDesc.SampleDesc.Quality = 0;

    HRESULT hr = device->CreateGraphicsPipelineState(
        &pipelineDesc,
        IID_PPV_ARGS(createdPipeline->pipelineState.GetAddressOf()));
    if (FAILED(hr))
    {
        LOG_DEBUG("CreateGraphicsPipelineState failed. hr=0x%08X", static_cast<unsigned int>(hr));
        DescribePipelineDesc(desc);;
        return hr;
    }

    *outPipeline = createdPipeline;
    return S_OK;
}

UINT PipelineLibrary::GetShaderCompileFlags() const
{

#if _DEBUG
	return D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
	return 0;
#endif
}

///====================================================
/// <summary>
/// パイプラインの説明を生成
/// </summary>
/// <param name="desc"></param>
/// <returns></returns>
///====================================================
std::string PipelineLibrary::DescribePipelineDesc(const PipelineDesc& desc) const
{
    auto WStringToString = [](const std::wstring& w) -> std::string {
        if (w.empty())
        {
            return {};
        }

        const int requiredSize = WideCharToMultiByte(
            CP_UTF8,
            0,
            w.c_str(),
            static_cast<int>(w.size()),
            nullptr,
            0,
            nullptr,
            nullptr);
        if (requiredSize <= 0)
        {
            return {};
        }

        std::string result(static_cast<size_t>(requiredSize), '\0');
        WideCharToMultiByte(
            CP_UTF8,
            0,
            w.c_str(),
            static_cast<int>(w.size()),
            result.data(),
            requiredSize,
            nullptr,
            nullptr);
        return result;
    };

    std::string description;
    description += "Vertex Shader: " + WStringToString(desc.vertexShader.m_ShaderFile) + "\n";
    description += "Vertex Entry Point: " + desc.vertexShader.m_EntryPoint + "\n";
    description += "Vertex Shader Model: " + desc.vertexShader.m_ShaderModel + "\n";
    description += "Pixel Shader: " + WStringToString(desc.pixelShader.m_ShaderFile) + "\n";
    description += "Pixel Entry Point: " + desc.pixelShader.m_EntryPoint + "\n";
    description += "Pixel Shader Model: " + desc.pixelShader.m_ShaderModel + "\n";

    return description;
}
