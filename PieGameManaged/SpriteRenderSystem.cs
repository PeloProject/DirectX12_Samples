internal sealed class SpriteRenderSystem
{
    private readonly TextureAssetManager _textureAssetManager = new TextureAssetManager();

    public void Initialize(GameScene scene)
    {
        foreach (Entity entity in scene.Entities)
        {
            EnsureNativeSprite(entity);
        }
    }

    public void Sync(GameScene scene)
    {
        foreach (Entity entity in scene.Entities)
        {
            SpriteRendererComponent? spriteRenderer = entity.SpriteRenderer;
            if (spriteRenderer == null)
            {
                continue;
            }

            EnsureNativeSprite(entity);
            if (spriteRenderer.NativeQuadHandle == 0)
            {
                continue;
            }

            if (!string.IsNullOrWhiteSpace(spriteRenderer.Material) &&
                !string.Equals(spriteRenderer.Material, spriteRenderer.AppliedMaterial, System.StringComparison.Ordinal))
            {
                NativeMethods.SetGameQuadMaterial(spriteRenderer.NativeQuadHandle, spriteRenderer.Material);
                spriteRenderer.AppliedMaterial = spriteRenderer.Material;
            }

            if (!string.IsNullOrWhiteSpace(spriteRenderer.Texture) &&
                !string.Equals(spriteRenderer.Texture, spriteRenderer.AppliedTexture, System.StringComparison.Ordinal))
            {
                if (spriteRenderer.TextureHandle.IsValid)
                {
                    _textureAssetManager.Release(spriteRenderer.TextureHandle);
                    spriteRenderer.TextureHandle = TextureHandle.Invalid;
                }

                spriteRenderer.TextureHandle = _textureAssetManager.Acquire(spriteRenderer.Texture);
                if (spriteRenderer.TextureHandle.IsValid)
                {
                    NativeMethods.SetGameQuadTextureHandle(spriteRenderer.NativeQuadHandle, spriteRenderer.TextureHandle.Value);
                }
                spriteRenderer.AppliedTexture = spriteRenderer.Texture;
            }

            NativeMethods.SetGameQuadTransform(
                spriteRenderer.NativeQuadHandle,
                entity.Transform.CenterX,
                entity.Transform.CenterY,
                entity.Transform.Width,
                entity.Transform.Height);
        }
    }

    public void Release(GameScene scene)
    {
        foreach (Entity entity in scene.Entities)
        {
            SpriteRendererComponent? spriteRenderer = entity.SpriteRenderer;
            if (spriteRenderer == null || spriteRenderer.NativeQuadHandle == 0)
            {
                continue;
            }

            NativeMethods.DestroyGameQuad(spriteRenderer.NativeQuadHandle);
            spriteRenderer.NativeQuadHandle = 0;
            spriteRenderer.AppliedTexture = string.Empty;
            spriteRenderer.AppliedMaterial = string.Empty;
            if (spriteRenderer.TextureHandle.IsValid)
            {
                _textureAssetManager.Release(spriteRenderer.TextureHandle);
            }
            spriteRenderer.TextureHandle = TextureHandle.Invalid;
        }

        _textureAssetManager.Clear();
    }

    private static void EnsureNativeSprite(Entity entity)
    {
        SpriteRendererComponent? spriteRenderer = entity.SpriteRenderer;
        if (spriteRenderer == null || spriteRenderer.NativeQuadHandle != 0)
        {
            return;
        }

        spriteRenderer.NativeQuadHandle = NativeMethods.CreateGameQuad();
    }
}
