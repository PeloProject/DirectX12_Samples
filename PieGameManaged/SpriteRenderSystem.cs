internal sealed class SpriteRenderSystem
{
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
        }
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
