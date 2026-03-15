using System.Collections.Generic;

internal sealed class TextureAssetManager
{
    private readonly Dictionary<string, TextureHandle> _handlesByPath = new Dictionary<string, TextureHandle>(System.StringComparer.Ordinal);
    private readonly Dictionary<TextureHandle, int> _refCounts = new Dictionary<TextureHandle, int>();

    public TextureHandle Acquire(string texturePath)
    {
        if (string.IsNullOrWhiteSpace(texturePath))
        {
            return TextureHandle.Invalid;
        }

        if (_handlesByPath.TryGetValue(texturePath, out TextureHandle handle))
        {
            _refCounts[handle] = _refCounts[handle] + 1;
            return handle;
        }

        handle = new TextureHandle(NativeMethods.AcquireTextureHandle(texturePath));
        if (handle.IsValid)
        {
            _handlesByPath[texturePath] = handle;
            _refCounts[handle] = 1;
        }
        return handle;
    }

    public void Release(TextureHandle handle)
    {
        if (!handle.IsValid || !_refCounts.TryGetValue(handle, out int refCount))
        {
            return;
        }

        if (refCount > 1)
        {
            _refCounts[handle] = refCount - 1;
            NativeMethods.ReleaseTextureHandle(handle.Value);
            return;
        }

        string? keyToRemove = null;
        foreach (KeyValuePair<string, TextureHandle> pair in _handlesByPath)
        {
            if (pair.Value == handle)
            {
                keyToRemove = pair.Key;
                break;
            }
        }

        if (keyToRemove != null)
        {
            _handlesByPath.Remove(keyToRemove);
        }

        _refCounts.Remove(handle);
        NativeMethods.ReleaseTextureHandle(handle.Value);
    }

    public void Clear()
    {
        foreach (KeyValuePair<TextureHandle, int> pair in _refCounts)
        {
            for (int i = 0; i < pair.Value; ++i)
            {
                NativeMethods.ReleaseTextureHandle(pair.Key.Value);
            }
        }

        _handlesByPath.Clear();
        _refCounts.Clear();
    }
}
