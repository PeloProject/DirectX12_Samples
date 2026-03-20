#pragma once

#include <d3d12.h>
#include <mutex>

class RootSignatureCache final
{
public:
    enum class RootParameterType : unsigned int
    {
        DescriptorTableSrv,
        ConstantBufferView
    };

    struct RootSignatureParameter
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

        bool operator==(const RootSignatureParameter& other) const;
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

    struct RootSignatureDesc
    {
        D3D12_ROOT_SIGNATURE_FLAGS flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
        std::vector<RootSignatureParameter> rootSignatureParameters;
        std::vector<StaticSamplerDesc> staticSamplers;


        bool operator==(const RootSignatureDesc& other) const;
    };
	
    struct RootSignatureDescHasher
    {
        size_t operator()(const RootSignatureDesc& desc) const noexcept;
    };

    static bool GetOrCreate(
        ID3D12Device* device,
        const RootSignatureDesc& desc,
		Microsoft::WRL::ComPtr<ID3D12RootSignature>* outRootSignature);

private:
    static std::mutex m_mutex;
    static std::unordered_map<RootSignatureDesc, Microsoft::WRL::ComPtr<ID3D12RootSignature>, RootSignatureDescHasher> m_Cache;

};