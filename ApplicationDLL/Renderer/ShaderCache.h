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

    enum class ShaderEntryState
    {
		InFlight,
		Success,
		Failed,
	};

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

    /// <summary>
    /// ShaderProgramDesc の内容に基づいてハッシュ値を計算する関数オブジェクトの呼び出し演算子。
    /// </summary>
    struct ShaderProgramDescHasher
    {
        size_t operator()(const ShaderProgramDesc& desc) const noexcept;
    };

    struct ShaderCacheEntry
    {
        ShaderEntryState m_State = ShaderEntryState::InFlight;
        Microsoft::WRL::ComPtr<ID3DBlob> m_ShaderBlob;
		std::condition_variable m_Condition;
	};

    static bool GetorCreate(
        const ShaderProgramDesc& desc,
        Microsoft::WRL::ComPtr<ID3DBlob>* outShaderBlob);

private:
    static std::mutex m_mutex;
	static std::unordered_map<ShaderProgramDesc, std::shared_ptr<ShaderCacheEntry>, ShaderProgramDescHasher> m_Cache;


};
