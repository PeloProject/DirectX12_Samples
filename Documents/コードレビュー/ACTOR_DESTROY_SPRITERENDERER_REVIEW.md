# Actor破棄時 SpriteRenderer 残留対応コードレビュー

## 対象

- `PieGameManaged/GameEntry.cs`
- `PieGameManaged/Scene.cs`
- `PieGameManaged/GameObject.cs`
- `PieGameManaged/SpriteRendererSystem.cs`
- `ApplicationDLL/Scene/SceneGame.h`

## レビュー概要

Player を一定時間後に破棄し、Actor 破棄後も SpriteRenderer の描画が残る問題を再現できた点は正しい。

初回レビュー時点では `Scene` が `NativeMethods.DestroySpriteRenderer()` を直接呼ぶ形になっており、`SpriteRendererSystem` が管理している native handle、適用済みマテリアル、適用済みテクスチャ、`TextureAssetManager` の状態更新を迂回していた。

その後の修正で、`Scene` は destroyed component list を持つだけになり、`SpriteRendererSystem.Sync()` の先頭で destroyed component を処理する形へ改善された。

現在は `Scene` が native renderer API や `SpriteRendererSystem` を直接知らない構造になっており、主な責務分離の問題は解消されている。

## 指摘事項

### High: Scene が native SpriteRenderer を直接破棄している

対象: `PieGameManaged/Scene.cs`

状態: 修正済み

```csharp
if (component is SpriteRenderer spriteRenderer)
{
    NativeMethods.DestroySpriteRenderer(spriteRenderer.NativeSpriteRendererHandle);
}
```

`Scene` が native renderer API を直接知る設計になっている。

この処理は既存の `SpriteRendererSystem.DestroyNativeSpriteRenderer()` を迂回するため、以下が実行されない。

- `spriteRenderer.NativeSpriteRendererHandle = 0`
- `spriteRenderer.AppliedTexture = string.Empty`
- `spriteRenderer.AppliedMaterial = string.Empty`
- `TextureAssetManager.Release(spriteRenderer.TextureHandle)`
- `spriteRenderer.TextureHandle = TextureHandle.Invalid`

結果として、native object は破棄されたように見えても、managed component 側には古い handle と texture 状態が残る。

修正方針:

- `Scene` は destroyed component を記録するだけにする
- `SpriteRendererSystem` に `Release(SpriteRenderer spriteRenderer)` を追加する
- `SpriteRendererSystem` が destroyed component list を処理し、既存の `DestroyNativeSpriteRenderer()` を再利用する

確認結果:

- `Scene` から `NativeMethods.DestroySpriteRenderer()` の直接呼び出しは削除された
- `Scene` から `GameEntry.SpriteRendererSystem` への依存も削除された
- `SpriteRendererSystem.Sync(scene)` の先頭で `ProcessDestroyedComponents(scene)` を呼ぶ形になった
- `SpriteRendererSystem.Release(SpriteRenderer)` は既存の `DestroyNativeSpriteRenderer()` を再利用している

この High 指摘は、現在の差分では解消済みと判断する。

### High: GameStop 時に queued component の OnDestroy が呼ばれない

対象: `PieGameManaged/GameEntry.cs`, `PieGameManaged/Scene.cs`

現在の `GameStop()` は以下の順序になっている。

```csharp
_spriteRendererSystem.Release(_scene);
_scene.DestroyAllGameObjects();
```

一方で、変更後の `DestroyAllGameObjects()` は component を destroyed list に積むだけで、`ProcessDestroyedComponents()` は `Scene.Update()` 内でしか呼ばれない。

そのため、PIE 停止時には `DestroyAllGameObjects()` の後に `Scene.Update()` が走らず、`Component.InvokeDestroy()` が呼ばれないまま `_scene = null` になる。

修正方針:

- `DestroyAllGameObjects()` では lifecycle 通知を確実に完了させる
- もしくは `GameStop()` で destroyed component list を明示処理する
- renderer release と component destroy の順序を明文化する

推奨順序:

```text
GameStop
  -> SpriteRendererSystem.ProcessDestroyedComponents(scene)
  -> SpriteRendererSystem.Release(scene)
  -> Scene.DestroyAllGameObjects()
  -> Scene.InvokePendingDestroyCallbacks()
```

実装を簡単に保つなら、`DestroyAllGameObjects()` は従来どおり `InvokeDestroy()` を呼び、同時に destroyed component list へ登録する形がよい。

### High: RemoveComponent 経路が destroyed component list に通知していない

対象: `PieGameManaged/GameObject.cs`

`GameObject.RemoveComponent<T>()` と `GameObject.RemoveComponent(Component)` は、現在も直接 `component.InvokeDestroy()` を呼んでいる。

このため、Actor 破棄では SpriteRenderer を回収できても、SpriteRenderer component 単体削除では native SpriteRenderer が残る。

修正方針:

- `RemoveComponent` でも `Scene.RegisterDestroyedComponent(component)` を呼ぶ
- `InvokeDestroy()` のタイミングを Actor 破棄と Component 削除で統一する

### Medium: 再現用 static state が GameStart でリセットされない

対象: `PieGameManaged/GameEntry.cs`

`_elapsedTime` と `_playerDestroyed` は static field だが、`GameStart()` で初期化されていない。

1回目の PIE 実行で `_playerDestroyed = true` になった後、次回 PIE 実行では Player 自動破棄の再現コードが動かない可能性がある。

修正方針:

```csharp
_elapsedTime = 3.0f;
_playerDestroyed = false;
_player = null;
```

上記を `GameStart()` の先頭、または `CreateScene()` の前に入れる。

### Medium: Player 破棄が Sync 後に実行されている

対象: `PieGameManaged/GameEntry.cs`

現在の `GameTick()` は以下の順序になっている。

```csharp
_scene.Update(deltaSeconds);
_spriteRendererSystem.Sync(_scene);
// この後に Player を Destroy
```

この順序だと、Player を破棄したフレームでは `SpriteRendererSystem.Sync()` がすでに終わっているため、native resource の解放は次フレーム以降になる。

1フレーム遅延を仕様として許容するなら問題ないが、Task 1 系の比較用コードとしては観測が分かりにくくなる。

修正方針:

- Player 破棄処理を `Sync()` より前に置く
- `SpriteRendererSystem.Sync()` の先頭で destroyed component list を処理する

推奨順序:

```text
GameTick
  -> scene.Update(deltaSeconds)
  -> test destroy player
  -> spriteRendererSystem.ProcessDestroyedComponents(scene)
  -> spriteRendererSystem.Sync(scene)
```

または `ProcessDestroyedComponents(scene)` を `Sync(scene)` の先頭に含める。

### Low: 無関係な native header 差分が含まれている

対象: `ApplicationDLL/Scene/SceneGame.h`

```cpp
//std::unique_ptr<PMDRenderObject> m_ppmdModel_;
```

今回の managed SpriteRenderer lifecycle 修正とは直接関係がないように見える。

意図がなければ、今回の差分から外した方がよい。

## 再レビュー後の残り指摘

### Low: 未使用 using が追加されている

対象: `PieGameManaged/SpriteRendererSystem.cs`

```csharp
using System.ComponentModel.Design.Serialization;
```

この using は現在の `SpriteRendererSystem` では使われていない。

修正方針:

- 削除する

### Low: destroyed component list の field 名が C# 命名規則から外れている

対象: `PieGameManaged/Scene.cs`

```csharp
private List<Component> _DestroyedComponents = new List<Component>();
```

private field は lower camel case にするのが C# の一般的な規約。

また、list 自体を差し替える必要がないなら `readonly` にできる。

修正方針:

```csharp
private readonly List<Component> _destroyedComponents = new List<Component>();
```

### Low: ClearDestroyedComponents の公開範囲が広い

対象: `PieGameManaged/Scene.cs`

```csharp
public void ClearDestroyedComponents()
```

`ClearDestroyedComponents()` は engine 内部の system から呼ぶための API であり、gameplay 側へ公開する必要は薄い。

修正方針:

```csharp
internal void ClearDestroyedComponents()
```

`DestroyedComponents` と同じく `internal` に揃える。

### Medium: Actor破棄と Component単体削除で lifecycle タイミングが不一致

対象: `PieGameManaged/Scene.cs`, `PieGameManaged/GameObject.cs`

現在の Actor 破棄経路では、`DestroyGameObjectComponents()` が component を destroyed list に積むだけで、`component.InvokeDestroy()` は `SpriteRendererSystem.Sync()` で destroyed list が処理されるまで遅延する。

一方で `GameObject.RemoveComponent<T>()` と `GameObject.RemoveComponent(Component)` は、従来どおり即時に `component.InvokeDestroy()` を呼ぶ。

このため、以下の2経路で lifecycle の意味が揃っていない。

```text
Scene.DestroyGameObject(actor)
  -> destroyed list に積む
  -> 後続の SpriteRendererSystem.Sync() で Release + InvokeDestroy

GameObject.RemoveComponent(component)
  -> 即 InvokeDestroy
  -> destroyed list へは積まない
```

問題:

- Actor 破棄時と Component 単体削除時で `OnDestroy()` のタイミングが異なる
- Component 単体削除では native SpriteRenderer release がまだ漏れる
- 今後 MeshRenderer / AudioSource / PhysicsBody を追加したとき、破棄経路ごとの差が増える

修正方針:

- `RemoveComponent` でも `Scene.RegisterDestroyedComponent(component)` 相当を呼ぶ
- `InvokeDestroy()` のタイミングを Actor 破棄と Component 削除で統一する
- destroyed component の native release は各 System が担当する

推奨:

```text
GameObject.RemoveComponent(component)
  -> component list から外す
  -> Scene.RegisterDestroyedComponent(component)

Scene.DestroyGameObject(actor)
  -> Scene.RegisterDestroyedComponent(component) for each component
  -> Scene.GameObjects から actor を外す

SpriteRendererSystem.Sync(scene)
  -> destroyed SpriteRenderer を Release
  -> destroyed component の OnDestroy lifecycle を確定
```

`OnDestroy()` を Scene が呼ぶか、destroyed component processing の共通 pass が呼ぶかは、後続タスクで統一して決める。

## 推奨設計

今回の設計では、責務を以下のように分けるのがよい。

```text
Scene
  - Actor / GameObject の lifetime を管理する
  - destroyed component list を持つ
  - native renderer API は直接呼ばない

GameObject
  - Component の追加・削除を管理する
  - Component 削除時に Scene へ destroyed component を通知する

SpriteRendererSystem
  - SpriteRenderer の native handle を作成・同期・解放する
  - destroyed component list から SpriteRenderer だけを処理する
  - TextureAssetManager の整合性を保つ

Component.OnDestroy
  - gameplay 側の終了通知として使う
  - native renderer resource の直接解放はしない
```

## 推奨 API 例

### Scene

```csharp
private readonly List<Component> _destroyedComponents = new List<Component>();

internal IReadOnlyList<Component> DestroyedComponents => _destroyedComponents;

internal void RegisterDestroyedComponent(Component component)
{
    _destroyedComponents.Add(component);
}

internal void ClearDestroyedComponents()
{
    _destroyedComponents.Clear();
}
```

### SpriteRendererSystem

```csharp
public void ProcessDestroyedComponents(Scene scene)
{
    foreach (Component component in scene.DestroyedComponents)
    {
        if (component is SpriteRenderer spriteRenderer)
        {
            Release(spriteRenderer);
        }
    }

    scene.ClearDestroyedComponents();
}

public void Release(SpriteRenderer spriteRenderer)
{
    DestroyNativeSpriteRenderer(spriteRenderer);
}
```

### GameTick

```csharp
_scene.Update(deltaSeconds);

// Test-only destroy code.
if (!_playerDestroyed && _elapsedTime <= 0.0f && _player != null)
{
    _scene.DestroyGameObject(_player);
    _player = null;
    _playerDestroyed = true;
}

_spriteRendererSystem.Sync(_scene);
```

`Sync()` の先頭で `ProcessDestroyedComponents(scene)` を呼べば、呼び忘れを防ぎやすい。

## 次に直す順序

1. `SpriteRendererSystem.cs` の未使用 using を削除する
2. `_DestroyedComponents` を `_destroyedComponents` にリネームし、可能なら `readonly` にする
3. `ClearDestroyedComponents()` を `internal` にする
4. `DestroyGameObject` と `RemoveComponent` の両方から destroyed component を登録する
5. `OnDestroy()` の呼び出しタイミングを Actor 破棄と Component 削除で統一する
6. `GameStop` 時の `OnDestroy` と renderer release の順序を整理する
7. 再現用の `_elapsedTime`, `_playerDestroyed`, `_player` を `GameStart()` でリセットする
8. `ApplicationDLL/Scene/SceneGame.h` の無関係な差分を外す

## 判定

修正方針は正しい方向に進んでいる。

初回レビューで指摘した `Scene` が renderer backend の破棄責務を持っている問題は、現在の修正で解消されている。

現在の残課題は、Actor 破棄と Component 単体削除の lifecycle を揃えること。

`Scene` は destroyed component list の所有者に留める方針でよい。実際の native resource 解放は、引き続き各 System に置くべき。
