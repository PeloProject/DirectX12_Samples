internal sealed class Entity
{
    public Entity(string name)
    {
        Name = name;
    }

    public string Name { get; }

    public TransformComponent Transform { get; } = new TransformComponent();

    public SpriteRendererComponent? SpriteRenderer { get; set; }
}
