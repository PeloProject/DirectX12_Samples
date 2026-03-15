using System;
using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;

internal static class GameEntry
{
    private static Scene? _scene = null;
    private static SpriteRendererSystem? _spriteRendererSystem;

    [UnmanagedCallersOnly(EntryPoint = "GameStart", CallConvs = new[] { typeof(CallConvCdecl) })]
    public static void GameStart()
    {
        _scene = CreateScene();
        _scene.Start();
        _spriteRendererSystem = new SpriteRendererSystem();
        _spriteRendererSystem.Initialize(_scene);

        NativeMethods.SetGameClearColor(0.08f, 0.08f, 0.12f, 1.0f);
        _spriteRendererSystem.Sync(_scene);
    }

    [UnmanagedCallersOnly(EntryPoint = "GameTick", CallConvs = new[] { typeof(CallConvCdecl) })]
    public static void GameTick(float deltaSeconds)
    {
        if (_scene == null || _spriteRendererSystem == null)
        {
            return;
        }

        _scene.Update(deltaSeconds);
        _spriteRendererSystem.Sync(_scene);
    }

    [UnmanagedCallersOnly(EntryPoint = "GameStop", CallConvs = new[] { typeof(CallConvCdecl) })]
    public static void GameStop()
    {
        NativeMethods.SetGameClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        if (_scene != null && _spriteRendererSystem != null)
        {
            _spriteRendererSystem.Release(_scene);
            _scene.DestroyAllGameObjects();
        }

        _spriteRendererSystem = null;
        _scene = null;
    }

    private static Scene CreateScene()
    {
        var scene = new Scene();
        GameObject player = scene.CreateGameObject("Player");
        SpriteRenderer spriteRenderer = player.AddComponent<SpriteRenderer>();
        spriteRenderer.Material = BuiltInMaterials.UnlitTexture;
        spriteRenderer.Texture = "player.png";
        player.AddComponent<PlayerPulseController>();

        return scene;
    }
}
