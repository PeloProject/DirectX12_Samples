#include "pch.h"
#include "TextureAssetManager.h"

TextureAssetManager& TextureAssetManager::Get()
{
    static TextureAssetManager instance;
    return instance;
}

TextureHandle TextureAssetManager::AcquireTexture(const char* texturePath)
{
    if (texturePath == nullptr || texturePath[0] == '\0')
    {
        return 0;
    }

    std::lock_guard<std::mutex> lock(mutex_);
    const std::string path(texturePath);
    const auto it = handlesByPath_.find(path);
    if (it != handlesByPath_.end())
    {
        auto textureIt = texturesByHandle_.find(it->second);
        if (textureIt != texturesByHandle_.end())
        {
            ++textureIt->second.refCount;
        }
        return it->second;
    }

    auto texture = std::make_shared<DX12Texture>();
    if (!texture->LoadFromFile(std::wstring(path.begin(), path.end())))
    {
        return 0;
    }

    const TextureHandle handle = nextHandle_++;
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
    nextHandle_ = 1;
}
