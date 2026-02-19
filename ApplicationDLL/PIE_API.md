# PIE (ApplicationDLLHost 起動) 仕様

## 目的

- 起動エディタ: `ApplicationDLLHost.exe`
- PIE開始時: `PieGameManaged.dll` (C# NativeAOT) を同一プロセスで読み込み
- Viewport: C#ゲームの更新結果を反映

## C#ゲームモジュール要件

`PieGameManaged.dll` に以下エクスポートが必要です。

- `GameStart()`
- `GameTick(float deltaSeconds)`
- `GameStop()`

## ApplicationDLL 側の公開API

- `StartPie()`
- `StopPie()`
- `IsPieRunning()`
- `SetGameClearColor(float r, float g, float b, float a)`  
  C#ゲームから呼び出し可能。Viewport描画のクリアカラーに反映されます。

## サンプルプロジェクト

- `PieGameManaged/PieGameManaged.csproj`
- `PieGameManaged/GameEntry.cs`

上記は NativeAOT で `PieGameManaged.dll` を生成する最小サンプルです。

## ビルド手順 (サンプル)

1. `dotnet publish PieGameManaged/PieGameManaged.csproj -c Debug`
2. 生成された `PieGameManaged.dll` を `ApplicationDLLHost.exe` と同じフォルダへ配置
3. `ApplicationDLLHost` 起動後、PIE ボタンで開始/停止

## Added API: Quad transform (for PolygonTest)
- `CreateGameQuad() -> uint32_t`
  - Creates a `PolygonTest` instance and returns its handle. Returns `0` on failure.
- `DestroyGameQuad(uint32_t handle)`
  - Destroys the `PolygonTest` instance for the handle.
- `SetGameQuadTransform(uint32_t handle, float centerX, float centerY, float width, float height)`
  - Updates transform in NDC space for the specified quad instance.
  - `width`/`height` values are clamped to a minimum of `0.01f` on native side.

## Hot Reload (PieGameManaged)
- While PIE is running, `ApplicationDLL` checks the source `PieGameManaged.dll` timestamp every 0.5 seconds.
- If updated, it unloads the old game module, loads the latest one, and restarts PIE game callbacks automatically.
- The module is loaded from a temp copy (`%TEMP%/DirectX12Samples/PieHotReload`) to avoid file-locking the original output.
- Auto NativeAOT publish:
  - While PIE is running, source changes under `PieGameManaged` (`.cs`, `.csproj`, `.props`, `.targets`, `.json`) are monitored.
  - On change, `dotnet publish PieGameManaged.csproj -c Debug -r win-x64` runs automatically.
  - After publish completes, the existing hot reload path loads the updated native module automatically.
