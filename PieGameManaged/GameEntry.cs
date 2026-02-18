using System;
using System.Runtime.InteropServices;

internal static class GameEntry
{
    private static float _time;

    [DllImport("ApplicationDLL.dll", CallingConvention = CallingConvention.Cdecl)]
    private static extern void SetGameClearColor(float r, float g, float b, float a);

    [UnmanagedCallersOnly(EntryPoint = "GameStart")]
    public static void GameStart()
    {
        _time = 0.0f;
        SetGameClearColor(0.08f, 0.08f, 0.12f, 1.0f);
    }

    [UnmanagedCallersOnly(EntryPoint = "GameTick")]
    public static void GameTick(float deltaSeconds)
    {
        _time += deltaSeconds;
        float pulse = 0.5f + 0.5f * MathF.Sin(_time * 1.5f);
        float r = 0.1f + 0.5f * pulse;
        float g = 0.08f + 0.25f * pulse;
        float b = 0.14f + 0.6f * (1.0f - pulse);
        SetGameClearColor(r, g, b, 1.0f);
    }

    [UnmanagedCallersOnly(EntryPoint = "GameStop")]
    public static void GameStop()
    {
        SetGameClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    }
}
