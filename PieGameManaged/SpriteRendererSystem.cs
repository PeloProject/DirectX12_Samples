using System.Collections.Generic;

internal sealed class SpriteRendererSystem
{
    private readonly TextureAssetManager _textureAssetManager = new TextureAssetManager();

    public void Initialize(Scene scene)
    {
        foreach (GameObject gameObject in scene.GameObjects)
        {
            foreach (SpriteRenderer spriteRenderer in gameObject.GetComponents<SpriteRenderer>())
            {
                EnsureNativeSpriteRenderer(spriteRenderer);
            }
        }
    }

    public void Sync(Scene scene)
    {
        foreach (GameObject gameObject in scene.GameObjects)
        {
            IReadOnlyList<SpriteRenderer> spriteRenderers = gameObject.GetComponents<SpriteRenderer>();
            if (spriteRenderers.Count == 0)
            {
                continue;
            }

            foreach (SpriteRenderer spriteRenderer in spriteRenderers)
            {
                if (!gameObject.ActiveSelf || !spriteRenderer.Enabled)
                {
                    DestroyNativeSpriteRenderer(spriteRenderer);
                    continue;
                }

                EnsureNativeSpriteRenderer(spriteRenderer);
                if (spriteRenderer.NativeSpriteRendererHandle == 0)
                {
                    continue;
                }

                if (!string.IsNullOrWhiteSpace(spriteRenderer.Material) &&
                    !string.Equals(spriteRenderer.Material, spriteRenderer.AppliedMaterial, System.StringComparison.Ordinal))
                {
                    NativeMethods.SetSpriteRendererMaterial(spriteRenderer.NativeSpriteRendererHandle, spriteRenderer.Material);
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
                        NativeMethods.SetSpriteRendererTexture(spriteRenderer.NativeSpriteRendererHandle, spriteRenderer.TextureHandle.Value);
                    }
                    spriteRenderer.AppliedTexture = spriteRenderer.Texture;
                }

                NativeMethods.SetSpriteRendererTransform(
                    spriteRenderer.NativeSpriteRendererHandle,
                    gameObject.Transform.CenterX,
                    gameObject.Transform.CenterY,
                    gameObject.Transform.Width,
                    gameObject.Transform.Height);
            }
        }
    }

    public void Release(Scene scene)
    {
        foreach (GameObject gameObject in scene.GameObjects)
        {
            foreach (SpriteRenderer spriteRenderer in gameObject.GetComponents<SpriteRenderer>())
            {
                DestroyNativeSpriteRenderer(spriteRenderer);
            }
        }

        _textureAssetManager.Clear();
    }

    private static void EnsureNativeSpriteRenderer(SpriteRenderer spriteRenderer)
    {
        if (spriteRenderer.NativeSpriteRendererHandle != 0)
        {
            return;
        }

        spriteRenderer.NativeSpriteRendererHandle = NativeMethods.CreateSpriteRenderer();
    }

    private void DestroyNativeSpriteRenderer(SpriteRenderer spriteRenderer)
    {
        if (spriteRenderer.NativeSpriteRendererHandle == 0)
        {
            return;
        }

        NativeMethods.DestroySpriteRenderer(spriteRenderer.NativeSpriteRendererHandle);
        spriteRenderer.NativeSpriteRendererHandle = 0;
        spriteRenderer.AppliedTexture = string.Empty;
        spriteRenderer.AppliedMaterial = string.Empty;
        if (spriteRenderer.TextureHandle.IsValid)
        {
            _textureAssetManager.Release(spriteRenderer.TextureHandle);
        }
        spriteRenderer.TextureHandle = TextureHandle.Invalid;
    }
}
