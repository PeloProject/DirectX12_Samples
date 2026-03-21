using System.Collections.Generic;

internal sealed class SpriteRendererSystem
{
    private readonly TextureAssetManager _textureAssetManager = new TextureAssetManager();

    ///=============================================================================================================================
    /// <summary>
    /// 初期化します。シーン内の全てのゲームオブジェクトをループして、スプライトレンダラーコンポーネントを持つものを探し、ネイティブのスプライトレンダラーを作成します。
    /// </summary>
    /// <param name="scene"></param>
    ///=============================================================================================================================
    public void Initialize(Scene scene)
    {
        foreach (GameObject gameObject in scene.GameObjects)
        {
            foreach (SpriteRenderer spriteRenderer in gameObject.GetComponents<SpriteRenderer>())
            {
                CreateNativeSpriteRenderer(spriteRenderer);
            }
        }
    }

    ///=============================================================================================================================
    /// <summary>
    /// スプライトレンダラーの状態をネイティブ側と同期します。非アクティブなゲームオブジェクトやスプライトレンダラーはネイティブ側のスプライトレンダラーを破棄します。
    /// </summary>
    /// <param name="scene"></param>
    ///=============================================================================================================================
    public void Sync(Scene scene)
    {
        // シーン状の全てのゲームオブジェクトをループして、スプライトレンダラーコンポーネントを持つものを探します。
        foreach (GameObject gameObject in scene.GameObjects)
        {
            IReadOnlyList<SpriteRenderer> spriteRenderers = gameObject.GetComponents<SpriteRenderer>();
            if (spriteRenderers.Count == 0)
            {
                continue;
            }

            foreach (SpriteRenderer spriteRenderer in spriteRenderers)
            {
                // ゲームオブジェクトが非アクティブであるか、スプライトレンダラーが無効である場合は、ネイティブのスプライトレンダラーを破棄します。
                if (!gameObject.ActiveSelf || !spriteRenderer.Enabled)
                {
                    DestroyNativeSpriteRenderer(spriteRenderer);
                    continue;
                }

                // ネイティブのスプライトレンダラーが存在しない場合は作成します。
                if (spriteRenderer.NativeSpriteRendererHandle == 0)
                {
                    CreateNativeSpriteRenderer(spriteRenderer);
                    if (spriteRenderer.NativeSpriteRendererHandle == 0)
                    {
                        continue;
                    }
                }

                // マテリアルが変更されている場合はネイティブ側に反映します。
                if (IsValidMaterial(spriteRenderer))
                {
                    NativeMethods.SetSpriteRendererMaterial(spriteRenderer.NativeSpriteRendererHandle, spriteRenderer.Material);
                    spriteRenderer.AppliedMaterial = spriteRenderer.Material;
                }

                // テクスチャが変更されている場合は、古いテクスチャを解放してから新しいテクスチャをネイティブ側に反映します。
                if (IsValidTexture(spriteRenderer))
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

                // ゲームオブジェクトのトランスフォームが変更されているかどうかを判断するためのロジックはここでは省略していますが、必要に応じて追加できます。
                NativeMethods.SetSpriteRendererTransform(
                    spriteRenderer.NativeSpriteRendererHandle,
                    gameObject.Transform.CenterX,
                    gameObject.Transform.CenterY,
                    gameObject.Transform.Width,
                    gameObject.Transform.Height);
            }
        }
    }

    ///=============================================================================================================================
    /// <summary>
    /// 破棄するシーン内の全てのスプライトレンダラーのネイティブリソースを解放します。
    /// </summary>
    /// <param name="scene"></param>
    ///=============================================================================================================================
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

    ///=============================================================================================================================
    /// <summary>
    /// テクスチャが有効かどうかを判断します。テクスチャがnull、空白、またはすでに適用されているテクスチャと同じ場合は無効とみなします。
    /// </summary>
    /// <param name="spriteRenderer"></param>
    /// <returns></returns>
    ///=============================================================================================================================
    private bool IsValidTexture(SpriteRenderer spriteRenderer)
    {
        if (string.IsNullOrWhiteSpace(spriteRenderer.Texture))
        {
            return false;
        }
        if (string.Equals(spriteRenderer.Texture, spriteRenderer.AppliedTexture, System.StringComparison.Ordinal))
        {
            return false;
        }
        return true;
    }

    ///=============================================================================================================================
    /// <summary>
    /// マテリアルが有効かどうかを判断します。マテリアルがnull、空白、またはすでに適用されているマテリアルと同じ場合は無効とみなします。
    /// </summary>
    /// <param name="spriteRenderer"></param>
    /// <returns></returns>
    ///=============================================================================================================================
    private bool IsValidMaterial(SpriteRenderer spriteRenderer)
    {
        if (string.IsNullOrWhiteSpace(spriteRenderer.Material))
        {
            return false;
        }
        if(string.Equals(spriteRenderer.Material, spriteRenderer.AppliedMaterial, System.StringComparison.Ordinal))
        {
            return false;
        }
        return true;
    }

    ///=============================================================================================================================
    /// <summary>
    /// ネイティブのスプライトレンダラーを作成します。すでにネイティブのスプライトレンダラーが存在する場合は何もしません。
    /// </summary>
    /// <param name="spriteRenderer"></param>
    ///=============================================================================================================================
    private static void CreateNativeSpriteRenderer(SpriteRenderer spriteRenderer)
    {
        if (spriteRenderer.NativeSpriteRendererHandle != 0)
        {
            return;
        }

        spriteRenderer.NativeSpriteRendererHandle = NativeMethods.CreateSpriteRenderer();
    }

    ///=============================================================================================================================
    /// <summary>
    /// スプライトレンダラーのネイティブリソースを解放します。ネイティブのスプライトレンダラーが存在しない場合は何もしません。
    /// </summary>
    /// <param name="spriteRenderer"></param>
    ///=============================================================================================================================
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
