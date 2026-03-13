#include "pch.h"
#include "ShaderCompiler.h"

#include <array>
#include <filesystem>

namespace
{
std::filesystem::path GetModuleDirectory()
{
    wchar_t modulePath[MAX_PATH] = {};
    HMODULE hModule = GetModuleHandleW(L"ApplicationDLL.dll");
    if (hModule != nullptr && GetModuleFileNameW(hModule, modulePath, MAX_PATH) > 0)
    {
        return std::filesystem::path(modulePath).parent_path();
    }

    wchar_t currentDir[MAX_PATH] = {};
    GetCurrentDirectoryW(MAX_PATH, currentDir);
    return std::filesystem::path(currentDir);
}

std::filesystem::path ResolveShaderPath(const wchar_t* shaderFileName)
{
    const std::filesystem::path moduleDir = GetModuleDirectory();
    const std::filesystem::path currentDir = std::filesystem::current_path();
    const std::array<std::filesystem::path, 4> candidates = {
        moduleDir / L".." / L".." / L".." / L".." / L"ApplicationDLL" / L"Shader" / shaderFileName,
        moduleDir / L"ApplicationDLL" / L"Shader" / shaderFileName,
        moduleDir / L"Shader" / shaderFileName,
        std::filesystem::path(L"Shader") / shaderFileName
    };
    const std::array<std::filesystem::path, 3> extraCandidates = {
        moduleDir / L".." / L"ApplicationDLL" / L"Shader" / shaderFileName,
        currentDir / L"ApplicationDLL" / L"Shader" / shaderFileName,
        currentDir / L"Shader" / shaderFileName
    };

    for (const auto& path : candidates)
    {
        std::error_code ec;
        if (std::filesystem::exists(path, ec))
        {
            return std::filesystem::weakly_canonical(path, ec);
        }
    }

    for (const auto& path : extraCandidates)
    {
        std::error_code ec;
        if (std::filesystem::exists(path, ec))
        {
            return std::filesystem::weakly_canonical(path, ec);
        }
    }

    return extraCandidates.back();
}
}

HRESULT ShaderCompiler::CompileFromFile(
    const wchar_t* shaderFileName,
    const char* entryPoint,
    const char* shaderModel,
    UINT compileFlags,
    Microsoft::WRL::ComPtr<ID3DBlob>& outBlob)
{
    if (shaderFileName == nullptr || entryPoint == nullptr || shaderModel == nullptr)
    {
        return E_INVALIDARG;
    }

    outBlob.Reset();

    const std::filesystem::path shaderPath = ResolveShaderPath(shaderFileName);
    Microsoft::WRL::ComPtr<ID3DBlob> errorBlob;
    const HRESULT hr = D3DCompileFromFile(
        shaderPath.c_str(),
        nullptr,
        D3D_COMPILE_STANDARD_FILE_INCLUDE,
        entryPoint,
        shaderModel,
        compileFlags,
        0,
        outBlob.GetAddressOf(),
        errorBlob.GetAddressOf());

    if (FAILED(hr))
    {
        if (errorBlob)
        {
            LOG_DEBUG("Shader Compile Error (%ls): %s", shaderFileName, static_cast<const char*>(errorBlob->GetBufferPointer()));
        }
        LOG_DEBUG("Shader Path: %ls", shaderPath.c_str());
    }

    return hr;
}

HRESULT ShaderCompiler::CompileBasicShaders(
    Microsoft::WRL::ComPtr<ID3DBlob>& outVertexShaderBlob,
    Microsoft::WRL::ComPtr<ID3DBlob>& outPixelShaderBlob)
{
    constexpr UINT kCompileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;

    HRESULT hr = CompileFromFile(
        L"BasicVertexShader.hlsl",
        "BasicVS",
        "vs_5_0",
        kCompileFlags,
        outVertexShaderBlob);
    if (FAILED(hr))
    {
        return hr;
    }

    hr = CompileFromFile(
        L"BasicPixelShader.hlsl",
        "BasicPS",
        "ps_5_0",
        kCompileFlags,
        outPixelShaderBlob);
    if (FAILED(hr))
    {
        return hr;
    }

    return S_OK;
}
