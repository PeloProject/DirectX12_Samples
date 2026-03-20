#pragma once
#include <d3d12.h>
#include <d3dcommon.h>
#include <wrl/client.h>
#include <string>
#include <unordered_map>
#include <mutex>

class ShaderCache final
{
public:
    /// <summary>
    /// シェーダープログラムの情報を保持する構造体。
    /// </summary>
    struct ShaderProgramDesc
    {
        std::wstring m_ShaderFile;
        std::string m_EntryPoint;
        std::string m_ShaderModel;
		UINT m_CompileFlags = 0;

        bool operator==(const ShaderProgramDesc& other) const;
    };

    struct ShaderProgramDescHasher
    {
        size_t operator()(const ShaderProgramDesc& desc) const noexcept;
    };

    static bool GetorCreate(
        const ShaderProgramDesc& desc,
        Microsoft::WRL::ComPtr<ID3DBlob>* outShaderBlob);

private:
    static std::mutex m_mutex;
	static std::unordered_map<ShaderProgramDesc, Microsoft::WRL::ComPtr<ID3DBlob>, ShaderProgramDescHasher> m_Cache;


};
