# ApplicationDLL PIE API

`ApplicationDLL.dll` は C# 側から PIE を制御できるよう、以下の C API を公開しています。

## Exports

- `void SetPieTickCallback(void(__cdecl* callback)(float deltaSeconds))`
- `void StartPie()`
- `void StopPie()`
- `BOOL IsPieRunning()`

## 想定フロー (C# 側)

1. `CreateNativeWindow()` / `ShowNativeWindow()` を呼ぶ。
2. `SetPieTickCallback()` で C# の Tick デリゲートを登録する。
3. `StartPie()` で PIE 開始。
4. 毎フレーム `MessageLoopIteration()` を呼ぶ。
5. `StopPie()` / `DestroyNativeWindow()` で終了。

## C# 宣言例

```csharp
[UnmanagedFunctionPointer(CallingConvention.Cdecl)]
public delegate void PieTickDelegate(float deltaSeconds);

[DllImport("ApplicationDLL.dll", CallingConvention = CallingConvention.Cdecl)]
public static extern void SetPieTickCallback(PieTickDelegate callback);

[DllImport("ApplicationDLL.dll", CallingConvention = CallingConvention.Cdecl)]
public static extern void StartPie();

[DllImport("ApplicationDLL.dll", CallingConvention = CallingConvention.Cdecl)]
public static extern void StopPie();

[DllImport("ApplicationDLL.dll", CallingConvention = CallingConvention.Cdecl)]
[return: MarshalAs(UnmanagedType.Bool)]
public static extern bool IsPieRunning();
```
