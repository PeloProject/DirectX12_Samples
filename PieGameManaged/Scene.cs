using System.Collections.Generic;

internal sealed class Scene
{
    public List<GameObject> GameObjects { get; } = new List<GameObject>();

    public bool IsStarted { get; private set; }

    public GameObject CreateGameObject(string name)
    {
        var gameObject = new GameObject(name)
        {
            Scene = this
        };
        GameObjects.Add(gameObject);
        if (IsStarted)
        {
            StartGameObject(gameObject);
        }
        return gameObject;
    }

    public bool DestroyGameObject(GameObject gameObject)
    {
        if (!GameObjects.Remove(gameObject))
        {
            return false;
        }

        DestroyGameObjectComponents(gameObject);
        return true;
    }

    public void DestroyAllGameObjects()
    {
        foreach (GameObject gameObject in GameObjects)
        {
            DestroyGameObjectComponents(gameObject);
        }

        GameObjects.Clear();
    }

    public void Start()
    {
        if (IsStarted)
        {
            return;
        }

        IsStarted = true;
        foreach (GameObject gameObject in GameObjects)
        {
            StartGameObject(gameObject);
        }
    }

    public void Update(float deltaSeconds)
    {
        if (!IsStarted)
        {
            return;
        }

        foreach (GameObject gameObject in GameObjects)
        {
            if (!gameObject.ActiveSelf)
            {
                continue;
            }

            foreach (Component component in gameObject.Components)
            {
                component.InvokeUpdate(deltaSeconds);
            }
        }
    }

    private static void StartGameObject(GameObject gameObject)
    {
        foreach (Component component in gameObject.Components)
        {
            component.InvokeStartIfNeeded();
        }
    }

    private static void DestroyGameObjectComponents(GameObject gameObject)
    {
        foreach (Component component in gameObject.Components)
        {
            component.InvokeDestroy();
        }
    }
}
