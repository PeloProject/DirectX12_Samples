internal sealed class SpriteRendererComponent
{
    public string Material { get; set; } = BuiltInMaterials.UnlitTexture;

    public string Texture { get; set; } = string.Empty;

    internal uint NativeQuadHandle { get; set; }

    internal string AppliedTexture { get; set; } = string.Empty;

    internal string AppliedMaterial { get; set; } = string.Empty;

    internal TextureHandle TextureHandle { get; set; } = TextureHandle.Invalid;
}
