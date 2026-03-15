using System;
using System.Runtime.InteropServices;

internal static class GameEntry
{
    private static float _time;
    private static GameScene? _scene;
    private static SpriteRenderSystem? _spriteRenderSystem;
    private static Entity? _player;

    [UnmanagedCallersOnly(EntryPoint = "GameStart")]
    public static void GameStart()
    {
        _time = 0.0f;
        _scene = CreateScene();
        _spriteRenderSystem = new SpriteRenderSystem();
        _spriteRenderSystem.Initialize(_scene);

        NativeMethods.SetGameClearColor(0.08f, 0.08f, 0.12f, 1.0f);
        _spriteRenderSystem.Sync(_scene);
    }

    [UnmanagedCallersOnly(EntryPoint = "GameTick")]
    public static void GameTick(float deltaSeconds)
    {
        if (_scene == null || _spriteRenderSystem == null || _player == null)
        {
            return;
        }

        _time += deltaSeconds;
        float pulse = 0.5f + 0.5f * MathF.Sin(_time * 1.5f);
        float r = 0.1f + 0.5f * pulse;
        float g = 0.08f + 0.25f * pulse;
        float b = 0.14f + 0.6f * (1.0f - pulse);
        NativeMethods.SetGameClearColor(r, g, b, 1.0f);

        _player.Transform.CenterX = 0.35f * MathF.Sin(_time * 0.9f);
        _player.Transform.CenterY = 0.18f * MathF.Cos(_time * 0.7f);
        _player.Transform.Width = 0.65f + 0.25f * pulse;
        _player.Transform.Height = 1.0f + 0.35f * (1.0f - pulse);

        _spriteRenderSystem.Sync(_scene);
    }

    [UnmanagedCallersOnly(EntryPoint = "GameStop")]
    public static void GameStop()
    {
        NativeMethods.SetGameClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        if (_scene != null && _spriteRenderSystem != null)
        {
            _spriteRenderSystem.Release(_scene);
        }

        _player = null;
        _spriteRenderSystem = null;
        _scene = null;
    }

    private static GameScene CreateScene()
    {
        var scene = new GameScene();
        _player = new Entity("Player")
        {
            SpriteRenderer = new SpriteRendererComponent
            {
                Material = BuiltInMaterials.UnlitTexture,
                Texture = "player.png"
            }
        };

        _player.Transform.CenterX = 0.0f;
        _player.Transform.CenterY = 0.0f;
        _player.Transform.Width = 0.8f;
        _player.Transform.Height = 1.4f;

        scene.Entities.Add(_player);
        return scene;
    }
}
