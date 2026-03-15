using System;

internal sealed class PlayerPulseController : Component
{
    private float _time;

    protected override void Start()
    {
        Transform.CenterX = 0.0f;
        Transform.CenterY = 0.0f;
        Transform.Width = 0.8f;
        Transform.Height = 1.4f;
    }

    protected override void Update(float deltaSeconds)
    {
        _time += deltaSeconds;

        float pulse = 0.5f + 0.5f * MathF.Sin(_time * 1.5f);
        float r = 0.1f + 0.5f * pulse;
        float g = 0.08f + 0.25f * pulse;
        float b = 0.14f + 0.6f * (1.0f - pulse);
        NativeMethods.SetGameClearColor(r, g, b, 1.0f);

        Transform.CenterX = 0.35f * MathF.Sin(_time * 0.9f);
        Transform.CenterY = 0.18f * MathF.Cos(_time * 0.7f);
        Transform.Width = 0.65f + 0.25f * pulse;
        Transform.Height = 1.0f + 0.35f * (1.0f - pulse);
    }
}
