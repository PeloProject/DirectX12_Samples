#include "ShaderCache.h"
#include <wrl/client.h>

#include "ShaderCompiler.h"
using Microsoft::WRL::ComPtr;

namespace
{
inline void HashCombine(size_t& seed, size_t value)
{
    seed ^= value + 0x9e3779b9 + (seed << 6) + (seed >> 2);
}
}

std::mutex ShaderCache::m_mutex;
std::unordered_map<ShaderCache::ShaderProgramDesc, std::shared_ptr<ShaderCache::ShaderCacheEntry>, ShaderCache::ShaderProgramDescHasher> ShaderCache::m_Cache;

/// <summary>
/// ShaderProgramDesc の等価比較演算子。メンバ変数 m_ShaderFile、m_EntryPoint、m_ShaderModel、m_CompileFlags の全てが等しい場合に true を返します。
/// </summary>
/// <param name="other">比較対象の ShaderProgramDesc オブジェクト。現在のオブジェクトの対応するメンバ変数と比較されます。</param>
/// <returns>すべての比較対象フィールドが等しい場合は true、そうでない場合は false を返します。</returns>
bool ShaderCache::ShaderProgramDesc::operator==(const ShaderCache::ShaderProgramDesc& other) const
{
    return m_ShaderFile == other.m_ShaderFile &&
        m_EntryPoint == other.m_EntryPoint &&
        m_ShaderModel == other.m_ShaderModel &&
        m_CompileFlags == other.m_CompileFlags;
}

size_t ShaderCache::ShaderProgramDescHasher::operator()(const ShaderCache::ShaderProgramDesc& desc) const noexcept
{
    size_t seed = 0;
    HashCombine(seed, std::hash<std::wstring>{}(desc.m_ShaderFile));
    HashCombine(seed, std::hash<std::string>{}(desc.m_EntryPoint));
    HashCombine(seed, std::hash<std::string>{}(desc.m_ShaderModel));
    HashCombine(seed, std::hash<UINT>{}(desc.m_CompileFlags));
    return seed;
}

/// <summary>
/// キャッシュからシェーダーブロブを取得し、存在しない場合はコンパイルしてキャッシュに保存する。
/// </summary>
/// <param name="desc">取得または生成するシェーダーを識別するための ShaderProgramDesc 構造体。</param>
/// <param name="outShaderBlob">取得または生成したシェーダーブロブを格納する Microsoft::WRL::ComPtr<ID3DBlob>* への出力ポインタ。成功時に有効なシェーダーブロブが設定される。</param>
/// <returns>操作に成功した場合は true を返し、取得やコンパイルに失敗した場合は false を返す。</returns>
bool ShaderCache::GetorCreate(
    const ShaderCache::ShaderProgramDesc& desc,
    Microsoft::WRL::ComPtr<ID3DBlob>* outShaderBlob)
{
    // キャッシュからシェーダーブロブを取得する処理をここに実装します。
    // もしキャッシュに存在しない場合は、シェーダーをコンパイルしてキャッシュに保存し、outShaderBlob に設定します。
    // 成功した場合は true を返し、失敗した場合は false を返します。
	if (outShaderBlob == nullptr)
    {
        return false;
    }

    std::shared_ptr<ShaderCache::ShaderCacheEntry> entry;
	bool isCreator = false;
    {
		std::unique_lock<std::mutex> lock(m_mutex);
		auto it = m_Cache.find(desc);
        if (it == m_Cache.end())
        {
            // キャッシュに存在しない場合は、シェーダーをコンパイルしてキャッシュに保存します。
            // まず、ShaderCacheEntry を作成してキャッシュに追加し、
            // その後ロックを解放してシェーダーのコンパイルを行います。
            // これにより、他のスレッドが同じシェーダーを要求した場合に、コンパイル中であることを認識できるようになります。
            entry = std::make_shared<ShaderCache::ShaderCacheEntry>();
            entry->m_State = ShaderEntryState::InFlight;
            m_Cache[desc] = entry;
            isCreator = true;
        }
		else
        {
            entry = it->second;
			if (entry->m_State == ShaderEntryState::InFlight)
            {
                // 別のスレッドが同じシェーダーをコンパイル中の場合は、コンパイルが完了するまで待機します。
                entry->m_Condition.wait(lock, [&entry = entry] { return entry->m_State != ShaderEntryState::InFlight; });
            }

            if (entry->m_State == ShaderEntryState::Failed)
            {
                // コンパイルに失敗している場合は、失敗を返します。
                return false;
            }
        }
    }


    if (isCreator)
    {
        HRESULT hr = ShaderCompiler::CompileFromFile(
            desc.m_ShaderFile.c_str(),
            desc.m_EntryPoint.c_str(),
            desc.m_ShaderModel.c_str(),
            desc.m_CompileFlags,
            entry->m_ShaderBlob);

		// コンパイルの結果に基づいてエントリの状態を更新します。
        // ロックを取得して状態を更新し、コンパイルに失敗した場合はエントリの状態を Failed に設定します。
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            if (FAILED(hr))
            {
                entry->m_State = ShaderEntryState::Failed;
            }
            else
            {
                entry->m_State = ShaderEntryState::Success;
            }
        }

		// コンパイルが完了したことを待機しているスレッドに通知します。
		entry->m_Condition.notify_all();

        if (FAILED(hr))
        {
            std::lock_guard<std::mutex> lock(m_mutex);
			m_Cache.erase(desc); // コンパイルに失敗したエントリをキャッシュから削除します。
            return false;
        }
    }

	*outShaderBlob = entry->m_ShaderBlob;
	return true;
}
