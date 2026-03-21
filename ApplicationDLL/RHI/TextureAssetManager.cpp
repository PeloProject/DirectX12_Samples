#include "pch.h"
#include "TextureAssetManager.h"

TextureAssetManager& TextureAssetManager::Get()
{
    static TextureAssetManager instance;
    return instance;
}

///==========================================================
/// <summary>
/// テクスチャーの取得と参照カウントの管理を行います。
/// 既に同じパスのテクスチャーが存在する場合はそのハンドルを返し、
/// 存在しない場合は新たにテクスチャーを読み込んでハンドルを生成します。
/// </summary>
/// <param name="texturePath"></param>
/// <returns></returns>
///==========================================================
TextureHandle TextureAssetManager::AcquireTexture(const char* texturePath)
{
    if (texturePath == nullptr || texturePath[0] == '\0')
    {
        return 0;
    }

    std::lock_guard<std::mutex> lock(mutex_);
    const std::string path(texturePath);
    const auto it = handlesByPath_.find(path);

	// 既に同じパスのテクスチャーが存在する場合は、そのハンドルを返し、参照カウントを増やします。
    if (it != handlesByPath_.end())
    {
        auto textureIt = texturesByHandle_.find(it->second);
        if (textureIt != texturesByHandle_.end())
        {
            ++textureIt->second.refCount;
        }
        return it->second;
    }

	// 同じパスのテクスチャーが存在しない場合は、新たにテクスチャーを読み込んでハンドルを生成します。
    auto texture = std::make_shared<DX12Texture>();
    if (!texture->LoadFromFile(std::wstring(path.begin(), path.end())))
    {
        return 0;
    }

	// 新しいテクスチャーエントリーを作成してマップに追加します。
    const TextureHandle handle = m_NextHandle++;
    handlesByPath_.emplace(path, handle);
    TextureEntry entry = {};
    entry.path = path;
    entry.texture = std::move(texture);
    entry.refCount = 1;
    texturesByHandle_.emplace(handle, std::move(entry));
    return handle;
}

void TextureAssetManager::ReleaseTexture(TextureHandle handle)
{
    if (handle == 0)
    {
        return;
    }

    std::lock_guard<std::mutex> lock(mutex_);
    const auto it = texturesByHandle_.find(handle);
    if (it == texturesByHandle_.end())
    {
        return;
    }

    if (it->second.refCount > 1)
    {
        --it->second.refCount;
        return;
    }

    handlesByPath_.erase(it->second.path);
    texturesByHandle_.erase(it);
}

std::shared_ptr<DX12Texture> TextureAssetManager::GetTexture(TextureHandle handle) const
{
    if (handle == 0)
    {
        return nullptr;
    }

    std::lock_guard<std::mutex> lock(mutex_);
    const auto it = texturesByHandle_.find(handle);
    if (it == texturesByHandle_.end())
    {
        return nullptr;
    }

    return it->second.texture;
}

void TextureAssetManager::Clear()
{
    std::lock_guard<std::mutex> lock(mutex_);
    handlesByPath_.clear();
    texturesByHandle_.clear();
    m_NextHandle = 1;
}
