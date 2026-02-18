using System.Diagnostics;

namespace Editor;

internal interface IPieGame
{
    void Start();
    void Tick(float deltaSeconds);
    void Stop();
}

internal sealed class PieGameHost
{
    private readonly IPieGame game = new SamplePieGame();
    private bool isRunning;

    public void Start()
    {
        if (isRunning)
        {
            return;
        }

        isRunning = true;
        game.Start();
    }

    public void Tick(float deltaSeconds)
    {
        if (!isRunning)
        {
            return;
        }

        game.Tick(deltaSeconds);
    }

    public void Stop()
    {
        if (!isRunning)
        {
            return;
        }

        game.Stop();
        isRunning = false;
    }
}

internal sealed class SamplePieGame : IPieGame
{
    private float elapsed;
    private int frameCount;

    public void Start()
    {
        elapsed = 0.0f;
        frameCount = 0;
        Debug.WriteLine("[PIE] C# Game Start");
    }

    public void Tick(float deltaSeconds)
    {
        elapsed += deltaSeconds;
        frameCount++;

        // 1秒ごとにログを出し、PIE実行を確認できるようにする
        if (frameCount % 60 == 0)
        {
            Debug.WriteLine($"[PIE] Tick elapsed={elapsed:F2}s");
        }
    }

    public void Stop()
    {
        Debug.WriteLine($"[PIE] C# Game Stop total={elapsed:F2}s");
    }
}
