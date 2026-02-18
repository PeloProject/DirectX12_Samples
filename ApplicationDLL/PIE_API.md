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
