#include "pch.h"
#include "RootSignatureCache.h"
#include <wrl/client.h>

std::mutex RootSignatureCache::m_mutex;
std::unordered_map<
    RootSignatureCache::RootSignatureDesc,
    Microsoft::WRL::ComPtr<ID3D12RootSignature>,
    RootSignatureCache::RootSignatureDescHasher> RootSignatureCache::m_Cache;

namespace
{
    inline void HashCombine(size_t& seed, size_t value)
    {
        seed ^= value + 0x9e3779b9 + (seed << 6) + (seed >> 2);
    }
}

bool RootSignatureCache::RootSignatureParameter::operator==(const RootSignatureParameter& other) const
{
    return type == other.type &&
        shaderVisibility == other.shaderVisibility &&
        numDescriptors == other.numDescriptors &&
        baseShaderRegister == other.baseShaderRegister &&
        registerSpace == other.registerSpace &&
        cbvShaderRegister == other.cbvShaderRegister &&
        cbvRegisterSpace == other.cbvRegisterSpace;
}

bool RootSignatureCache::StaticSamplerDesc::operator==(const RootSignatureCache::StaticSamplerDesc& other) const
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


bool RootSignatureCache::RootSignatureDesc::operator==(const RootSignatureDesc& other) const
{
    return flags == other.flags &&
        rootSignatureParameters == other.rootSignatureParameters &&
		staticSamplers == other.staticSamplers;
}

size_t RootSignatureCache::RootSignatureDescHasher::operator()(const RootSignatureCache::RootSignatureDesc& desc) const noexcept
{
    size_t seed = 0;
    for (const auto& root : desc.rootSignatureParameters)
    {
        HashCombine(seed, std::hash<int>{}(static_cast<int>(root.type)));
        HashCombine(seed, std::hash<int>{}(static_cast<int>(root.shaderVisibility)));
        HashCombine(seed, std::hash<UINT>{}(root.numDescriptors));
        HashCombine(seed, std::hash<UINT>{}(root.baseShaderRegister));
        HashCombine(seed, std::hash<UINT>{}(root.registerSpace));
        HashCombine(seed, std::hash<UINT>{}(root.cbvShaderRegister));
        HashCombine(seed, std::hash<UINT>{}(root.cbvRegisterSpace));
    }
    return seed;
}

bool RootSignatureCache::GetOrCreate(
    ID3D12Device* device,
    const RootSignatureDesc& desc,
    Microsoft::WRL::ComPtr<ID3D12RootSignature>* outRootSignature)
{
    // ルートシグネチャの作成ロジックをここに実装します。
    // 例えば、D3D12_ROOT_SIGNATURE_DESC を構築し、D3D12SerializeRootSignature と ID3D12Device::CreateRootSignature を使用してルートシグネチャを作成します。
    // キャッシュ機能を実装する場合は、内部のハッシュマップを使用して既存のルートシグネチャを検索し、存在しない場合にのみ新しいルートシグネチャを作成します。
    if (outRootSignature == nullptr)
    {
        return false;
    }
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_Cache.find(desc);
        if (it != m_Cache.end())
        {
            *outRootSignature = it->second;
            return true;
        }
    }

    // ルートシグネチャの構築
    D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc = {};
    rootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

    // ルートパラメータの構築。DescriptorTableSrv タイプのパラメータは D3D12_DESCRIPTOR_RANGE を使用して記述され、
    // RootParameter の DescriptorTable メンバに関連付けられます。
    // CBV タイプのパラメータは RootParameter の Descriptor メンバを直接使用して記述されます。
    std::vector<D3D12_DESCRIPTOR_RANGE> descriptorRanges;
    descriptorRanges.reserve(desc.rootSignatureParameters.size());
    std::vector<D3D12_ROOT_PARAMETER> rootParameters(desc.rootSignatureParameters.size());
    for (size_t i = 0; i < desc.rootSignatureParameters.size(); ++i)
    {
        const auto& source = desc.rootSignatureParameters[i];
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
    HRESULT hr = D3D12SerializeRootSignature(
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

    Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature;
    hr = device->CreateRootSignature(
        0,
        rootSignatureBlob->GetBufferPointer(),
        rootSignatureBlob->GetBufferSize(),
        IID_PPV_ARGS(rootSignature.GetAddressOf()));
    if (FAILED(hr))
    {
        return hr;
    }
    if (FAILED(hr))
    {
        return false;
    }

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_Cache[desc] = rootSignature;
    }

    *outRootSignature = rootSignature;
    return false; // 仮の戻り値。実際の実装では適切な値を返してください。
}