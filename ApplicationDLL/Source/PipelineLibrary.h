#pragma once

#include <d3d12.h>
#include <d3dcommon.h>
#include <dxgi1_6.h>
#include <wrl/client.h>

#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>

class PipelineLibrary final
{
public:
    struct PipelineKey
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

        bool operator==(const PipelineKey& other) const;
    };

    struct Pipeline
    {
        Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState;
        Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature;
    };

    HRESULT GetOrCreate(
        ID3D12Device* device,
        const PipelineKey& key,
        const D3D12_INPUT_ELEMENT_DESC* inputElements,
        UINT inputElementCount,
        std::shared_ptr<const Pipeline>* outPipeline);

    void Clear();

private:
    struct PipelineKeyHasher
    {
        size_t operator()(const PipelineKey& key) const;
    };

    HRESULT CreatePipeline(
        ID3D12Device* device,
        const PipelineKey& key,
        const D3D12_INPUT_ELEMENT_DESC* inputElements,
        UINT inputElementCount,
        std::shared_ptr<const Pipeline>* outPipeline) const;

private:
    mutable std::mutex mutex_;
    std::unordered_map<PipelineKey, std::shared_ptr<const Pipeline>, PipelineKeyHasher> cache_;
};
