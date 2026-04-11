# DirectX12_Samples

## Qt editor

`EditorQt` is the new editor shell. The previous `Editor` WPF host remains in the repository as a legacy reference, but the intended editor frontend is now Qt.

Build requirements:

- Qt 6 Widgets SDK
- CMake 3.21+
- `CMAKE_PREFIX_PATH` pointing to your Qt installation, for example `C:\Qt\6.8.0\msvc2022_64`

Build example:

```powershell
cd EditorQt
cmake --preset default
cmake --build --preset default
```

Design notes:

- `ApplicationDLL` can now run with its internal ImGui editor UI disabled.
- `EditorQt` owns the tool UI and drives the runtime through exported DLL functions.
- The current viewport embedding is still Win32-specific, but the editor chrome is now Qt-based and isolated from the engine runtime.

## PieGameManaged executable launch

`ApplicationDLLHost.exe` now supports a game mode that starts PIE automatically and shows only the game viewport.

- Editor mode (existing): `ApplicationDLLHost.exe`
- Game mode (new): `ApplicationDLLHost.exe --game`

Required files in the same folder:

- `ApplicationDLL.dll`
- `PieGameManaged.dll` (NativeAOT output)

## Optional: GLAD integration for OpenGL backend

`ApplicationDLL` now supports GLAD if the following files are present:

- `ApplicationDLL/ThirdParty/glad/include/glad/glad.h`
- `ApplicationDLL/ThirdParty/glad/src/glad.c`

When these files exist, `OpenGLRenderDevice` initializes GLAD automatically.
When they do not exist, the project falls back to system OpenGL headers (`gl/GL.h`).

`ApplicationDLL.vcxproj` already has a conditional compile entry for `glad.c`, so no extra project edit is required.
