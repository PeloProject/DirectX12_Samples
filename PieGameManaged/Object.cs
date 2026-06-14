using System.Collections.Generic;

public abstract class ObjectBase
{
    public string? Name {get; set;} = string.Empty;
    public bool IsValid { get; private set;} = true;

    public virtual void Destroy()
    {
        IsValid = false;
    }
}