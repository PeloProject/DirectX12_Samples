#include "pch.h"
#include "PipelineLibrary.h"

#include "ShaderCompiler.h"

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

bool PipelineLibrary::RootParameterDesc::operator==(const RootParameterDesc& other) const
{
    return type == other.type &&
        shaderVisibility == other.shaderVisibility &&
        numDescriptors == other.numDescriptors &&
        baseShaderRegister == other.baseShaderRegister &&
        registerSpace == other.registerSpace &&
        cbvShaderRegister == other.cbvShaderRegister &&
        cbvRegisterSpace == other.cbvRegisterSpace;
}

bool PipelineLibrary::StaticSamplerDesc::operator==(const StaticSamplerDesc& other) const
{
    return filter == other.filter &&
        addressU == other.addressU &&
        addressV == other.addressV &&
        addressW == other.addressW &&
        shaderRegister == other.shaderRegister &&
        registerSpace == other.registerSpace &&
        shaderVisibility == other.shaderVisibility &&
        comparisonFunc == other.comparisonFunc &&
        borderColor == other.borderColor &&
        mipLODBias == other.mipLODBias &&
        maxAnisotropy == other.maxAnisotropy &&
        minLOD == other.minLOD &&
        maxLOD == other.maxLOD;
}

bool PipelineLibrary::PipelineDesc::operator==(const PipelineDesc& other) const
{
    return vertexShaderFile == other.vertexShaderFile &&
        vertexEntryPoint == other.vertexEntryPoint &&
        vertexShaderModel == other.vertexShaderModel &&
        pixelShaderFile == other.pixelShaderFile &&
        pixelEntryPoint == other.pixelEntryPoint &&
        pixelShaderModel == other.pixelShaderModel &&
        renderTargetFormat == other.renderTargetFormat &&
        cullMode == other.cullMode &&
        topologyType == other.topologyType &&
        enableDepth == other.enableDepth &&
        enableBlend == other.enableBlend &&
        inputElements == other.inputElements &&
        rootParameters == other.rootParameters &&
        staticSamplers == other.staticSamplers;
}

size_t PipelineLibrary::PipelineDescHasher::operator()(const PipelineDesc& desc) const
{
    size_t seed = 0;
    HashCombine(seed, std::hash<std::wstring>{}(desc.vertexShaderFile));
    HashCombine(seed, std::hash<std::string>{}(desc.vertexEntryPoint));
    HashCombine(seed, std::hash<std::string>{}(desc.vertexShaderModel));
    HashCombine(seed, std::hash<std::wstring>{}(desc.pixelShaderFile));
    HashCombine(seed, std::hash<std::string>{}(desc.pixelEntryPoint));
    HashCombine(seed, std::hash<std::string>{}(desc.pixelShaderModel));
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
    for (const auto& root : desc.rootParameters)
    {
        HashCombine(seed, std::hash<int>{}(static_cast<int>(root.type)));
        HashCombine(seed, std::hash<int>{}(static_cast<int>(root.shaderVisibility)));
        HashCombine(seed, std::hash<UINT>{}(root.numDescriptors));
        HashCombine(seed, std::hash<UINT>{}(root.baseShaderRegister));
        HashCombine(seed, std::hash<UINT>{}(root.registerSpace));
        HashCombine(seed, std::hash<UINT>{}(root.cbvShaderRegister));
        HashCombine(seed, std::hash<UINT>{}(root.cbvRegisterSpace));
    }
    for (const auto& sampler : desc.staticSamplers)
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
    return seed;
}

HRESULT PipelineLibrary::GetOrCreate(
    ID3D12Device* device,
    const PipelineDesc& desc,
    std::shared_ptr<const Pipeline>* outPipeline)
{
	m_TotalRequestCount++;

    if (device == nullptr || outPipeline == nullptr || desc.inputElements.empty() || desc.rootParameters.empty())
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

    constexpr UINT kCompileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;

    // 頂点シェーダーのコンパイル
    Microsoft::WRL::ComPtr<ID3DBlob> vertexShaderBlob;
    HRESULT hr = ShaderCompiler::CompileFromFile(
        desc.vertexShaderFile.c_str(),
        desc.vertexEntryPoint.c_str(),
        desc.vertexShaderModel.c_str(),
        kCompileFlags,
        vertexShaderBlob);
    if (FAILED(hr))
    {
        return hr;
    }

	// ピクセルシェーダーのコンパイル
    Microsoft::WRL::ComPtr<ID3DBlob> pixelShaderBlob;
    hr = ShaderCompiler::CompileFromFile(
        desc.pixelShaderFile.c_str(),
        desc.pixelEntryPoint.c_str(),
        desc.pixelShaderModel.c_str(),
        kCompileFlags,
        pixelShaderBlob);
    if (FAILED(hr))
    {
        return hr;
    }

	// ルートシグネチャの構築
    D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc = {};
    rootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

	// ルートパラメータの構築。DescriptorTableSrv タイプのパラメータは D3D12_DESCRIPTOR_RANGE を使用して記述され、
    // RootParameter の DescriptorTable メンバに関連付けられます。
    // CBV タイプのパラメータは RootParameter の Descriptor メンバを直接使用して記述されます。
    std::vector<D3D12_DESCRIPTOR_RANGE> descriptorRanges;
    descriptorRanges.reserve(desc.rootParameters.size());
    std::vector<D3D12_ROOT_PARAMETER> rootParameters(desc.rootParameters.size());
    for (size_t i = 0; i < desc.rootParameters.size(); ++i)
    {
        const auto& source = desc.rootParameters[i];
        D3D12_ROOT_PARAMETER& target = rootParameters[i];
        target.ShaderVisibility = source.shaderVisibility;

        // シェーダーリソースビューのディスクリプタ設定
        if (source.type == RootParameterType::DescriptorTableSrv)
        {
            D3D12_DESCRIPTOR_RANGE range = {};
            range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
            range.NumDescriptors = source.numDescriptors;
            range.BaseShaderRegister = source.baseShaderRegister;
            range.RegisterSpace = source.registerSpace;
            range.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
            descriptorRanges.push_back(range);

            target.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
            target.DescriptorTable.NumDescriptorRanges = 1;
            target.DescriptorTable.pDescriptorRanges = &descriptorRanges.back();
        }
        else
        {
			// 定数バッファビューのディスクリプタ設定
            target.ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
            target.Descriptor.ShaderRegister = source.cbvShaderRegister;
            target.Descriptor.RegisterSpace = source.cbvRegisterSpace;
        }
    }
    rootSignatureDesc.NumParameters = static_cast<UINT>(rootParameters.size());
    rootSignatureDesc.pParameters = rootParameters.data();

	// スタティックサンプラーの構築。D3D12_STATIC_SAMPLER_DESC を使用して記述され、
    // RootSignatureDesc の pStaticSamplers メンバに関連付けられます。
    std::vector<D3D12_STATIC_SAMPLER_DESC> staticSamplers(desc.staticSamplers.size());
    for (size_t i = 0; i < desc.staticSamplers.size(); ++i)
    {
        const auto& source = desc.staticSamplers[i];
        D3D12_STATIC_SAMPLER_DESC& target = staticSamplers[i];
        target.Filter = source.filter;
        target.AddressU = source.addressU;
        target.AddressV = source.addressV;
        target.AddressW = source.addressW;
        target.MipLODBias = source.mipLODBias;
        target.MaxAnisotropy = source.maxAnisotropy;
        target.ComparisonFunc = source.comparisonFunc;
        target.BorderColor = source.borderColor;
        target.MinLOD = source.minLOD;
        target.MaxLOD = source.maxLOD;
        target.ShaderRegister = source.shaderRegister;
        target.RegisterSpace = source.registerSpace;
        target.ShaderVisibility = source.shaderVisibility;
    }
    rootSignatureDesc.NumStaticSamplers = static_cast<UINT>(staticSamplers.size());
    rootSignatureDesc.pStaticSamplers = staticSamplers.empty() ? nullptr : staticSamplers.data();

	// ルートシグネチャのシリアライズと作成。D3D12SerializeRootSignature を使用してルートシグネチャをシリアライズし、
    // その後 ID3D12Device::CreateRootSignature を呼び出してルートシグネチャオブジェクトを作成します。
    Microsoft::WRL::ComPtr<ID3DBlob> errorBlob;
    Microsoft::WRL::ComPtr<ID3DBlob> rootSignatureBlob;
    hr = D3D12SerializeRootSignature(
        &rootSignatureDesc,
        D3D_ROOT_SIGNATURE_VERSION_1,
        rootSignatureBlob.GetAddressOf(),
        errorBlob.GetAddressOf());
    if (FAILED(hr))
    {
        if (errorBlob)
        {
            LOG_DEBUG("Root Signature Serialize Error: %s", static_cast<const char*>(errorBlob->GetBufferPointer()));
        }
        return hr;
    }

    auto createdPipeline = std::make_shared<Pipeline>();
    hr = device->CreateRootSignature(
        0,
        rootSignatureBlob->GetBufferPointer(),
        rootSignatureBlob->GetBufferSize(),
        IID_PPV_ARGS(createdPipeline->rootSignature.GetAddressOf()));
    if (FAILED(hr))
    {
        return hr;
    }

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

    hr = device->CreateGraphicsPipelineState(
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
        // 簡易変換（ASCII / 基本ラテン文字が想定されるなら十分）
        return std::string(w.begin(), w.end());
    };

    std::string description;
    description += "Vertex Shader: " + WStringToString(desc.vertexShaderFile) + "\n";
    description += "Vertex Entry Point: " + desc.vertexEntryPoint + "\n";
    description += "Vertex Shader Model: " + desc.vertexShaderModel + "\n";
    description += "Pixel Shader: " + WStringToString(desc.pixelShaderFile) + "\n";
    description += "Pixel Entry Point: " + desc.pixelEntryPoint + "\n";
    description += "Pixel Shader Model: " + desc.pixelShaderModel + "\n";

    return description;
}