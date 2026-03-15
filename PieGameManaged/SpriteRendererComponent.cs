internal sealed class SpriteRendererComponent
{
    public string Material { get; set; } = BuiltInMaterials.UnlitTexture;

    public string Texture { get; set; } = string.Empty;

    internal uint NativeQuadHandle { get; set; }
}
