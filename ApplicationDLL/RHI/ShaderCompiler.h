#pragma once

#include <d3dcompiler.h>
#include <wrl/client.h>

class ShaderCompiler final
{
public:
    static HRESULT CompileFromFile(
        const wchar_t* shaderFileName,
        const char* entryPoint,
        const char* shaderModel,
        UINT compileFlags,
        Microsoft::WRL::ComPtr<ID3DBlob>& outBlob);

    static HRESULT CompileBasicShaders(
        Microsoft::WRL::ComPtr<ID3DBlob>& outVertexShaderBlob,
        Microsoft::WRL::ComPtr<ID3DBlob>& outPixelShaderBlob);
};
