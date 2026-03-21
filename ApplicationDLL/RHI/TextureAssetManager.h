#pragma once

#include "DX12Texture.h"

#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>

using TextureHandle = uint32_t;

class TextureAssetManager
{
public:
    struct TextureEntry
    {
        std::string path;
        std::shared_ptr<DX12Texture> texture;
        uint32_t refCount = 0;
    };

    static TextureAssetManager& Get();

    TextureAssetManager(const TextureAssetManager&) = delete;
    TextureAssetManager& operator=(const TextureAssetManager&) = delete;

    TextureHandle AcquireTexture(const char* texturePath);
    void ReleaseTexture(TextureHandle handle);
    std::shared_ptr<DX12Texture> GetTexture(TextureHandle handle) const;
    void Clear();

private:
    TextureAssetManager() = default;

private:
    mutable std::mutex mutex_;
    std::unordered_map<std::string, TextureHandle> handlesByPath_;
    std::unordered_map<TextureHandle, TextureEntry> texturesByHandle_;
    TextureHandle m_NextHandle = 1;
};
