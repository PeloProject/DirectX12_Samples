using System.Runtime.InteropServices;

internal static class NativeMethods
{
    [DllImport("ApplicationDLL.dll", CallingConvention = CallingConvention.Cdecl)]
    public static extern void SetGameClearColor(float r, float g, float b, float a);

    [DllImport("ApplicationDLL.dll", CallingConvention = CallingConvention.Cdecl)]
    public static extern uint CreateGameQuad();

    [DllImport("ApplicationDLL.dll", CallingConvention = CallingConvention.Cdecl)]
    public static extern void DestroyGameQuad(uint handle);

    [DllImport("ApplicationDLL.dll", CallingConvention = CallingConvention.Cdecl)]
    public static extern void SetGameQuadTransform(uint handle, float centerX, float centerY, float width, float height);
}
