using System.Runtime.InteropServices;

internal static class NativeMethods
{
    [DllImport("ApplicationDLL.dll", CallingConvention = CallingConvention.Cdecl)]
    public static extern void SetGameClearColor(float r, float g, float b, float a);

    [DllImport("ApplicationDLL.dll", CallingConvention = CallingConvention.Cdecl)]
    public static extern uint CreateSpriteRenderer();

    [DllImport("ApplicationDLL.dll", CallingConvention = CallingConvention.Cdecl)]
    public static extern void DestroySpriteRenderer(uint handle);

    [DllImport("ApplicationDLL.dll", CallingConvention = CallingConvention.Cdecl)]
    public static extern void SetSpriteRendererTransform(uint handle, float centerX, float centerY, float width, float height);

    [DllImport("ApplicationDLL.dll", CallingConvention = CallingConvention.Cdecl)]
    public static extern uint AcquireTextureHandle([MarshalAs(UnmanagedType.LPUTF8Str)] string texturePath);

    [DllImport("ApplicationDLL.dll", CallingConvention = CallingConvention.Cdecl)]
    public static extern void ReleaseTextureHandle(uint textureHandle);

    [DllImport("ApplicationDLL.dll", CallingConvention = CallingConvention.Cdecl)]
    public static extern void SetSpriteRendererTexture(uint handle, uint textureHandle);

    [DllImport("ApplicationDLL.dll", CallingConvention = CallingConvention.Cdecl)]
    public static extern void SetSpriteRendererMaterial(uint handle, [MarshalAs(UnmanagedType.LPUTF8Str)] string materialName);
}
