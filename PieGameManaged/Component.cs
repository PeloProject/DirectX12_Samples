internal abstract class Component
{
    private bool _isAwake;
    private bool _isStarted;

    public GameObject GameObject { get; internal set; } = null!;

    public Scene Scene => GameObject.Scene;

    public Transform Transform => GameObject.Transform;

    public bool Enabled { get; set; } = true;

    protected virtual void Awake()
    {
    }

    protected virtual void Start()
    {
    }

    protected virtual void Update(float deltaSeconds)
    {
    }

    protected virtual void OnDestroy()
    {
    }

    internal void InvokeAwakeIfNeeded()
    {
        if (_isAwake)
        {
            return;
        }

        _isAwake = true;
        Awake();
    }

    internal void InvokeStartIfNeeded()
    {
        if (_isStarted)
        {
            return;
        }

        InvokeAwakeIfNeeded();
        _isStarted = true;
        Start();
    }

    internal void InvokeUpdate(float deltaSeconds)
    {
        if (!_isStarted || !Enabled)
        {
            return;
        }

        Update(deltaSeconds);
    }

    internal void InvokeDestroy()
    {
        if (!_isAwake)
        {
            return;
        }

        OnDestroy();
        _isAwake = false;
        _isStarted = false;
    }
}
