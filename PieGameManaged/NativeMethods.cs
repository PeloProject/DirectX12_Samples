using System;
using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;

internal static unsafe class NativeMethods
{
    [StructLayout(LayoutKind.Sequential)]
    internal struct NativeApiTable
    {
        public delegate* unmanaged[Cdecl]<float, float, float, float, void> SetGameClearColor;
        public delegate* unmanaged[Cdecl]<uint> CreateSpriteRenderer;
        public delegate* unmanaged[Cdecl]<uint, void> DestroySpriteRenderer;
        public delegate* unmanaged[Cdecl]<uint, float, float, float, float, void> SetSpriteRendererTransform;
        public delegate* unmanaged[Cdecl]<byte*, uint> AcquireTextureHandle;
        public delegate* unmanaged[Cdecl]<uint, void> ReleaseTextureHandle;
        public delegate* unmanaged[Cdecl]<uint, uint, void> SetSpriteRendererTexture;
        public delegate* unmanaged[Cdecl]<uint, byte*, void> SetSpriteRendererMaterial;
    }

    private static NativeApiTable s_api;
    private static bool s_initialized;

    [UnmanagedCallersOnly(EntryPoint = "SetNativeApi", CallConvs = new[] { typeof(CallConvCdecl) })]
    public static void SetNativeApi(NativeApiTable* api)
    {
        if (api == null)
        {
            s_api = default;
            s_initialized = false;
            return;
        }

        s_api = *api;
        s_initialized = true;
    }

    public static void SetGameClearColor(float r, float g, float b, float a)
    {
        EnsureInitialized();
        s_api.SetGameClearColor(r, g, b, a);
    }

    public static uint CreateSpriteRenderer()
    {
        EnsureInitialized();
        return s_api.CreateSpriteRenderer();
    }

    public static void DestroySpriteRenderer(uint handle)
    {
        EnsureInitialized();
        s_api.DestroySpriteRenderer(handle);
    }

    public static void SetSpriteRendererTransform(uint handle, float centerX, float centerY, float width, float height)
    {
        EnsureInitialized();
        s_api.SetSpriteRendererTransform(handle, centerX, centerY, width, height);
    }

    public static uint AcquireTextureHandle(string texturePath)
    {
        EnsureInitialized();
        IntPtr utf8 = Marshal.StringToCoTaskMemUTF8(texturePath);
        try
        {
            return s_api.AcquireTextureHandle((byte*)utf8);
        }
        finally
        {
            Marshal.FreeCoTaskMem(utf8);
        }
    }

    public static void ReleaseTextureHandle(uint textureHandle)
    {
        EnsureInitialized();
        s_api.ReleaseTextureHandle(textureHandle);
    }

    public static void SetSpriteRendererTexture(uint handle, uint textureHandle)
    {
        EnsureInitialized();
        s_api.SetSpriteRendererTexture(handle, textureHandle);
    }

    public static void SetSpriteRendererMaterial(uint handle, string materialName)
    {
        EnsureInitialized();
        IntPtr utf8 = Marshal.StringToCoTaskMemUTF8(materialName);
        try
        {
            s_api.SetSpriteRendererMaterial(handle, (byte*)utf8);
        }
        finally
        {
            Marshal.FreeCoTaskMem(utf8);
        }
    }

    private static void EnsureInitialized()
    {
        if (!s_initialized)
        {
            throw new InvalidOperationException("Native PIE API is not initialized.");
        }
    }
}
