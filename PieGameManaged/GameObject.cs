using System.Collections.Generic;

internal sealed class GameObject
{
    private readonly List<Component> _components = new List<Component>();

    public GameObject(string name)
    {
        Name = name;
    }

    public string Name { get; }

    internal Scene Scene { get; set; } = null!;

    public bool ActiveSelf { get; set; } = true;

    public Transform Transform { get; } = new Transform();

    public T AddComponent<T>() where T : Component, new()
    {
        T component = new T
        {
            GameObject = this
        };
        component.InvokeAwakeIfNeeded();
        _components.Add(component);
        if (Scene.IsStarted)
        {
            component.InvokeStartIfNeeded();
        }
        return component;
    }

    public T? GetComponent<T>() where T : Component
    {
        foreach (Component component in _components)
        {
            if (component is T typedComponent)
            {
                return typedComponent;
            }
        }

        return null;
    }

    public IReadOnlyList<T> GetComponents<T>() where T : Component
    {
        var components = new List<T>();
        foreach (Component component in _components)
        {
            if (component is T typedComponent)
            {
                components.Add(typedComponent);
            }
        }

        return components;
    }

    ///==============================================================
    /// <summary>
    /// コンポーネントの削除
    /// </summary>
    /// <typeparam name="T"></typeparam>
    /// <returns></returns>
    ///==============================================================
    public bool RemoveComponent<T>() where T : Component
    {
        for (int i = 0; i < _components.Count; ++i)
        {
            if (_components[i] is T component)
            {
                // OnDestroy 内で RemoveComponent が呼ばれても index がずれないよう、先に list から外す。
                _components.RemoveAt(i);
                Scene.NotifyComponentDestroyed(component);
                return true;
            }
        }

        return false;
    }

    public bool RemoveComponent(Component component)
    {
        if (!_components.Remove(component))
        {
            return false;
        }

        Scene.NotifyComponentDestroyed(component);
        return true;
    }

    public IReadOnlyList<Component> Components => _components;
}
