#include "pch.h"
#include "PipelineLibrary.h"

#include "ShaderCompiler.h"

#include <vector>

namespace
{
inline void HashCombine(size_t& seed, size_t value)
{
    seed ^= value + 0x9e3779b9 + (seed << 6) + (seed >> 2);
}
}

bool PipelineLibrary::PipelineKey::operator==(const PipelineKey& other) const
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
        enableBlend == other.enableBlend;
}

size_t PipelineLibrary::PipelineKeyHasher::operator()(const PipelineKey& key) const
{
    size_t seed = 0;
    HashCombine(seed, std::hash<std::wstring>{}(key.vertexShaderFile));
    HashCombine(seed, std::hash<std::string>{}(key.vertexEntryPoint));
    HashCombine(seed, std::hash<std::string>{}(key.vertexShaderModel));
    HashCombine(seed, std::hash<std::wstring>{}(key.pixelShaderFile));
    HashCombine(seed, std::hash<std::string>{}(key.pixelEntryPoint));
    HashCombine(seed, std::hash<std::string>{}(key.pixelShaderModel));
    HashCombine(seed, std::hash<int>{}(static_cast<int>(key.renderTargetFormat)));
    HashCombine(seed, std::hash<int>{}(static_cast<int>(key.cullMode)));
    HashCombine(seed, std::hash<int>{}(static_cast<int>(key.topologyType)));
    HashCombine(seed, std::hash<bool>{}(key.enableDepth));
    HashCombine(seed, std::hash<bool>{}(key.enableBlend));
    return seed;
}

HRESULT PipelineLibrary::GetOrCreate(
    ID3D12Device* device,
    const PipelineKey& key,
    const D3D12_INPUT_ELEMENT_DESC* inputElements,
    UINT inputElementCount,
    std::shared_ptr<const Pipeline>* outPipeline)
{
    if (device == nullptr || inputElements == nullptr || inputElementCount == 0 || outPipeline == nullptr)
    {
        return E_INVALIDARG;
    }

    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = cache_.find(key);
        if (it != cache_.end())
        {
            *outPipeline = it->second;
            return S_OK;
        }
    }

    std::shared_ptr<const Pipeline> createdPipeline;
    HRESULT hr = CreatePipeline(device, key, inputElements, inputElementCount, &createdPipeline);
    if (FAILED(hr))
    {
        return hr;
    }

    std::lock_guard<std::mutex> lock(mutex_);
    auto [it, inserted] = cache_.emplace(key, createdPipeline);
    *outPipeline = it->second;
    (void)inserted;
    return S_OK;
}

void PipelineLibrary::Clear()
{
    std::lock_guard<std::mutex> lock(mutex_);
    cache_.clear();
}

HRESULT PipelineLibrary::CreatePipeline(
    ID3D12Device* device,
    const PipelineKey& key,
    const D3D12_INPUT_ELEMENT_DESC* inputElements,
    UINT inputElementCount,
    std::shared_ptr<const Pipeline>* outPipeline) const
{
    constexpr UINT kCompileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;

    Microsoft::WRL::ComPtr<ID3DBlob> vertexShaderBlob;
    HRESULT hr = ShaderCompiler::CompileFromFile(
        key.vertexShaderFile.c_str(),
        key.vertexEntryPoint.c_str(),
        key.vertexShaderModel.c_str(),
        kCompileFlags,
        vertexShaderBlob);
    if (FAILED(hr))
    {
        return hr;
    }

    Microsoft::WRL::ComPtr<ID3DBlob> pixelShaderBlob;
    hr = ShaderCompiler::CompileFromFile(
        key.pixelShaderFile.c_str(),
        key.pixelEntryPoint.c_str(),
        key.pixelShaderModel.c_str(),
        kCompileFlags,
        pixelShaderBlob);
    if (FAILED(hr))
    {
        return hr;
    }

    D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc = {};
    rootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

    D3D12_DESCRIPTOR_RANGE descriptorRange = {};
    descriptorRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    descriptorRange.NumDescriptors = 1;
    descriptorRange.BaseShaderRegister = 0;
    descriptorRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    D3D12_ROOT_PARAMETER rootParameter = {};
    rootParameter.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParameter.DescriptorTable.NumDescriptorRanges = 1;
    rootParameter.DescriptorTable.pDescriptorRanges = &descriptorRange;
    rootParameter.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    rootSignatureDesc.pParameters = &rootParameter;
    rootSignatureDesc.NumParameters = 1;

    D3D12_STATIC_SAMPLER_DESC samplerDesc = {};
    samplerDesc.Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
    samplerDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    samplerDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    samplerDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    samplerDesc.MaxLOD = D3D12_FLOAT32_MAX;
    samplerDesc.MinLOD = 0.0f;
    samplerDesc.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
    samplerDesc.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    rootSignatureDesc.pStaticSamplers = &samplerDesc;
    rootSignatureDesc.NumStaticSamplers = 1;

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

    D3D12_GRAPHICS_PIPELINE_STATE_DESC pipelineDesc = {};
    pipelineDesc.pRootSignature = createdPipeline->rootSignature.Get();
    pipelineDesc.VS.BytecodeLength = vertexShaderBlob->GetBufferSize();
    pipelineDesc.VS.pShaderBytecode = vertexShaderBlob->GetBufferPointer();
    pipelineDesc.PS.BytecodeLength = pixelShaderBlob->GetBufferSize();
    pipelineDesc.PS.pShaderBytecode = pixelShaderBlob->GetBufferPointer();
    pipelineDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;
    pipelineDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
    pipelineDesc.RasterizerState.CullMode = key.cullMode;
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
    renderTargetBlendDesc.BlendEnable = key.enableBlend ? TRUE : FALSE;
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

    pipelineDesc.DepthStencilState.DepthEnable = key.enableDepth ? TRUE : FALSE;
    pipelineDesc.DepthStencilState.DepthWriteMask = key.enableDepth ? D3D12_DEPTH_WRITE_MASK_ALL : D3D12_DEPTH_WRITE_MASK_ZERO;
    pipelineDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
    pipelineDesc.DepthStencilState.StencilEnable = FALSE;
    pipelineDesc.DepthStencilState.StencilReadMask = D3D12_DEFAULT_STENCIL_READ_MASK;
    pipelineDesc.DepthStencilState.StencilWriteMask = D3D12_DEFAULT_STENCIL_WRITE_MASK;
    pipelineDesc.DepthStencilState.FrontFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;
    pipelineDesc.DepthStencilState.FrontFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
    pipelineDesc.DepthStencilState.FrontFace.StencilPassOp = D3D12_STENCIL_OP_KEEP;
    pipelineDesc.DepthStencilState.FrontFace.StencilFunc = D3D12_COMPARISON_FUNC_ALWAYS;
    pipelineDesc.DepthStencilState.BackFace = pipelineDesc.DepthStencilState.FrontFace;

    std::vector<D3D12_INPUT_ELEMENT_DESC> inputLayout(inputElementCount);
    for (UINT i = 0; i < inputElementCount; ++i)
    {
        inputLayout[i] = inputElements[i];
    }

    pipelineDesc.InputLayout.pInputElementDescs = inputLayout.data();
    pipelineDesc.InputLayout.NumElements = static_cast<UINT>(inputLayout.size());
    pipelineDesc.IBStripCutValue = D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_DISABLED;
    pipelineDesc.PrimitiveTopologyType = key.topologyType;
    pipelineDesc.NumRenderTargets = 1;
    pipelineDesc.RTVFormats[0] = key.renderTargetFormat;
    pipelineDesc.SampleDesc.Count = 1;
    pipelineDesc.SampleDesc.Quality = 0;

    hr = device->CreateGraphicsPipelineState(
        &pipelineDesc,
        IID_PPV_ARGS(createdPipeline->pipelineState.GetAddressOf()));
    if (FAILED(hr))
    {
        LOG_DEBUG("CreateGraphicsPipelineState failed. hr=0x%08X", static_cast<unsigned int>(hr));
        return hr;
    }

    *outPipeline = createdPipeline;
    return S_OK;
}
