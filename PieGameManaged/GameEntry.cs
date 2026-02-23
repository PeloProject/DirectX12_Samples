using System;
using System.Runtime.InteropServices;

internal static class GameEntry
{
    private static float _time;
    private static uint _quadHandle;

    [DllImport("ApplicationDLL.dll", CallingConvention = CallingConvention.Cdecl)]
    private static extern void SetGameClearColor(float r, float g, float b, float a);

    [DllImport("ApplicationDLL.dll", CallingConvention = CallingConvention.Cdecl)]
    private static extern uint CreateGameQuad();

    [DllImport("ApplicationDLL.dll", CallingConvention = CallingConvention.Cdecl)]
    private static extern void DestroyGameQuad(uint handle);

    [DllImport("ApplicationDLL.dll", CallingConvention = CallingConvention.Cdecl)]
    private static extern void SetGameQuadTransform(uint handle, float centerX, float centerY, float width, float height);

    /// <summary>
    /// 初期化
    /// </summary>
    [UnmanagedCallersOnly(EntryPoint = "GameStart")]
    public static void GameStart()
    {
        _time = 0.0f;
        _quadHandle = CreateGameQuad();
        SetGameClearColor(0.08f, 0.08f, 0.12f, 1.0f);
        if (_quadHandle != 0)
        {
            SetGameQuadTransform(_quadHandle, 0.0f, 0.0f, 0.8f, 1.4f);
        }
    }

    [UnmanagedCallersOnly(EntryPoint = "GameTick")]
    public static void GameTick(float deltaSeconds)
    {
        if (_quadHandle == 0)
        {
            _quadHandle = CreateGameQuad();
        }

        _time += deltaSeconds;
        float pulse = 0.5f + 0.5f * MathF.Sin(_time * 1.5f);
        float r = 0.1f + 0.5f * pulse;
        float g = 0.08f + 0.25f * pulse;
        float b = 0.14f + 0.6f * (1.0f - pulse);
        SetGameClearColor(r, g, b, 1.0f);

        float centerX = 0.35f * MathF.Sin(_time * 0.9f);
        float centerY = 0.18f * MathF.Cos(_time * 0.7f);
        float width = 0.65f + 0.25f * pulse;
        float height = 1.0f + 0.35f * (1.0f - pulse);
        if (_quadHandle != 0)
        {
            SetGameQuadTransform(_quadHandle, centerX, centerY, width, height);
        }
    }

    [UnmanagedCallersOnly(EntryPoint = "GameStop")]
    public static void GameStop()
    {
        SetGameClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        if (_quadHandle != 0)
        {
            DestroyGameQuad(_quadHandle);
            _quadHandle = 0;
        }
    }
}
