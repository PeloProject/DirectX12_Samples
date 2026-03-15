internal readonly record struct TextureHandle(uint Value)
{
    public bool IsValid => Value != 0;

    public static TextureHandle Invalid => new TextureHandle(0);
}
