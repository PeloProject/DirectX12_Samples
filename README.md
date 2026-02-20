# DirectX12_Samples

## PieGameManaged executable launch

`ApplicationDLLHost.exe` now supports a game mode that starts PIE automatically and shows only the game viewport.

- Editor mode (existing): `ApplicationDLLHost.exe`
- Game mode (new): `ApplicationDLLHost.exe --game`

Required files in the same folder:

- `ApplicationDLL.dll`
- `PieGameManaged.dll` (NativeAOT output)
