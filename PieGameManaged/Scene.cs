using System.Collections.Generic;

internal sealed class Scene
{

    /// <summary>
    /// このシーンに存在する全てのゲームオブジェクトのリスト。
    /// ゲームオブジェクトはシーン内で一意である必要があるため、同じゲームオブジェクトを複数回追加することはできません。
    /// </summary>
    public List<GameObject> GameObjects { get; } = new List<GameObject>();

    private List<Component> _DestroyedComponents = new List<Component>();

    internal IReadOnlyList<Component> DestroyedComponents => _DestroyedComponents;

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

    private void DestroyGameObjectComponents(GameObject gameObject)
    {
        foreach (Component component in gameObject.Components)
        {
            _DestroyedComponents.Add(component);
        }
    }

    /// <summary>
    /// 削除されたオブジェクトコンポーネントのリストをクリアします。
    /// </summary>
    public void ClearDestroyedComponents()
    {
        _DestroyedComponents.Clear();
    }
}
