using System;
using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;

internal static class GameEntry
{
    private static Scene? _scene = null;
    private static SpriteRendererSystem? _spriteRendererSystem;
    public static SpriteRendererSystem? SpriteRendererSystem{ get {return _spriteRendererSystem;} }

    private static GameObject? _player = null;
    private static float _elapsedTime = 3.0f;
    private static bool _playerDestroyed = false;

    ///========================================================================================
    /// <summary>
    /// ゲーム開始時に呼び出されるエントリポイント。ゲームの初期化処理をここで行う。
    /// </summary>
    ///========================================================================================
    [UnmanagedCallersOnly(EntryPoint = "GameStart", CallConvs = new[] { typeof(CallConvCdecl) })]
    public static void GameStart()
    {
        _elapsedTime = 3.0f;
        _playerDestroyed = false;

        _scene = CreateScene();
        _scene.Start();
        _spriteRendererSystem = new SpriteRendererSystem();
        _spriteRendererSystem.Initialize(_scene);

        NativeMethods.SetGameClearColor(0.08f, 0.08f, 0.12f, 1.0f);
        _spriteRendererSystem.Sync(_scene);
    }

    ///========================================================================================
    /// <summary>
    /// ゲームの各フレームで呼び出されるエントリポイント。ゲームの更新処理をここで行う。
    /// </summary>
    /// <param name="deltaSeconds"></param>
    ///========================================================================================
    [UnmanagedCallersOnly(EntryPoint = "GameTick", CallConvs = new[] { typeof(CallConvCdecl) })]
    public static void GameTick(float deltaSeconds)
    {
        if (_scene == null || _spriteRendererSystem == null)
        {
            return;
        }

        _scene.Update(deltaSeconds);

        if (_elapsedTime > 0.0f)
        {
            _elapsedTime -= deltaSeconds;
        }
        else if (!_playerDestroyed)
        {
            if (_player != null)
            {
                _scene.DestroyGameObject(_player);
                _player = null;
                _playerDestroyed = true;
            }
        }

        _spriteRendererSystem.Sync(_scene);
    }

    ///========================================================================================
    /// <summary>
    /// ゲーム終了時に呼び出されるエントリポイント。ゲームのクリーンアップ処理をここで行う。
    /// </summary>
    ///========================================================================================
    [UnmanagedCallersOnly(EntryPoint = "GameStop", CallConvs = new[] { typeof(CallConvCdecl) })]
    public static void GameStop()
    {
        NativeMethods.SetGameClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        if (_scene != null && _spriteRendererSystem != null)
        {
            // 初めにNativeを削除した後にC#側を削除
            _spriteRendererSystem.ProcessDestroyedComponents(_scene);
            _spriteRendererSystem.Release(_scene);
            _scene.DestroyAllGameObjects();
        }

        _spriteRendererSystem = null;
        _scene = null;
    }

    ///========================================================================================
    /// <summary>
    /// シーンを作成するヘルパーメソッド。ゲームオブジェクトやコンポーネントの初期化をここで行う。
    /// </summary>
    /// <returns></returns>
    ///========================================================================================
    private static Scene CreateScene()
    {
        var scene = new Scene();

        // プレイヤーオブジェクトの作成
        _player = scene.CreateGameObject("Player");
        SpriteRenderer spriteRenderer = _player.AddComponent<SpriteRenderer>();
        spriteRenderer.Material = BuiltInMaterials.UnlitTexture;
        spriteRenderer.Texture = "textest.png";
        _player.AddComponent<PlayerPulseController>();

        return scene;
    }
}
