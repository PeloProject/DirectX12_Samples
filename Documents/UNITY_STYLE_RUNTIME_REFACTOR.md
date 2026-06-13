# Unity-Style Runtime Refactor Plan

## Goal

Refactor the runtime toward a Unity-like `GameObject + Component + Scene` model.

At the same time, remove the ambiguous `GameQuad` naming and replace it with names that match Unity concepts more closely.

This document defines:

- target runtime structure
- naming changes
- ownership and responsibilities
- native API shape
- incremental migration steps

## Current Problems

### 1. `GameQuad` mixes engine primitive and game concept

`GameQuad` is currently a renderable quad primitive, but the name reads like a game-level object.

- Engine side stores instances in `g_gameQuads`
- Managed side creates them through `CreateGameQuad`
- `SpriteRenderSystem` treats them as native sprite render objects

This causes a mismatch:

- the engine behaves like it owns game objects
- the managed layer behaves like it owns entities/components

### 2. Engine and gameplay responsibilities are blurred

Current split is:

- managed: `Entity`, `TransformComponent`, `SpriteRendererComponent`
- native: `IGameQuad`, native quad registry, render loop

This is workable, but it is halfway between:

- a low-level rendering API
- a Unity-style scene object model

### 3. Naming does not scale

If `GameQuad` remains the main abstraction, future additions become awkward:

- `GameQuadObject`
- `GameQuadMaterial`
- `GameQuadCamera`

Those names do not describe the actual abstraction boundary.

## Target Model

Adopt a Unity-like model with three layers.

### 1. Scene layer

Gameplay-facing scene graph and component ownership.

- `Scene`
- `GameObject`
- `Component`

This layer should be the source of truth for game state.

### 2. Engine object layer

Native runtime objects that back components.

- `TransformNode`
- `SpriteRenderer`
- `Camera`
- `MeshRenderer`

These are not gameplay objects. They are engine/runtime objects created from component state.

### 3. Rendering layer

Backend-specific draw primitives and renderer implementation.

- `ISpriteRendererBackend`
- `QuadRenderObject`
- `Material`
- `TextureHandle`

This layer should not know about `GameObject`.

## Naming Changes

### Managed side

Rename gameplay-facing types toward Unity naming.

| Current | Target |
|---|---|
| `Entity` | `GameObject` |
| `GameScene` | `Scene` or `SceneAsset` |
| `SpriteRendererComponent` | `SpriteRenderer` |
| `TransformComponent` | `Transform` |
| `SpriteRenderSystem` | `SceneSyncSystem` or `SpriteRendererSystem` |

Recommended minimum:

- `Entity` -> `GameObject`
- `GameScene` -> `Scene`
- `SpriteRendererComponent` -> `SpriteRenderer`
- `TransformComponent` -> `Transform`

### Native side

Rename render-facing types away from `GameQuad`.

| Current | Target |
|---|---|
| `IGameQuad` | `ISpriteRenderObject` |
| `CreateGameQuadForBackend` | `CreateSpriteRenderObjectForBackend` |
| `CreateGameQuad` | `CreateSpriteRenderer` |
| `DestroyGameQuad` | `DestroySpriteRenderer` |
| `SetGameQuadTransform` | `SetSpriteRendererTransform` |
| `SetGameQuadTextureHandle` | `SetSpriteRendererTexture` |
| `SetGameQuadMaterial` | `SetSpriteRendererMaterial` |
| `RenderGameQuads` | `RenderSpriteRenderers` |
| `g_gameQuads` | `g_spriteRenderers` |

Recommended minimum:

- keep the render primitive name explicit: `SpriteRenderObject`
- reserve `SpriteRenderer` for the engine-facing component concept

## Responsibility Split

### Managed `Scene`

Owns:

- `GameObject` list
- component composition
- scene serialization in the future

Does not own:

- backend render objects directly

### Managed `GameObject`

Owns:

- name
- active flag
- component list or typed component fields during transition
- `Transform`

Unity-compatible target shape:

```csharp
internal sealed class GameObject
{
    public string Name { get; }
    public bool ActiveSelf { get; set; } = true;
    public Transform Transform { get; } = new Transform();
    private readonly List<Component> _components = new();
}
```

For the first migration stage, typed fields are acceptable:

```csharp
internal sealed class GameObject
{
    public string Name { get; }
    public Transform Transform { get; } = new Transform();
    public SpriteRenderer? SpriteRenderer { get; set; }
}
```

### Managed `Component`

Base type for Unity-style expansion.

```csharp
internal abstract class Component
{
    public GameObject GameObject { get; internal set; } = null!;
    public Transform Transform => GameObject.Transform;
    public bool Enabled { get; set; } = true;
}
```

Derived example:

```csharp
internal sealed class SpriteRenderer : Component
{
    public string Material { get; set; } = BuiltInMaterials.UnlitTexture;
    public string Texture { get; set; } = string.Empty;

    internal uint NativeRendererHandle { get; set; }
    internal string AppliedTexture { get; set; } = string.Empty;
    internal string AppliedMaterial { get; set; } = string.Empty;
    internal TextureHandle TextureHandle { get; set; } = TextureHandle.Invalid;
}
```

### Native runtime

Native runtime should own engine instances, not game objects.

Good:

- native sprite renderer registry
- native camera registry
- native material and texture resources

Avoid:

- native `GameObject`
- native component tree mirroring the managed scene one-to-one in the first step

Reason:

- the managed scene is already the gameplay authority
- duplicating the full object model in both runtimes makes synchronization and lifetime much harder

## Recommended Architecture

### Short version

Use this split:

- managed: `Scene`, `GameObject`, `Component`
- native API: create/destroy/update engine components
- renderer: backend-specific draw objects

### Flow

1. Managed code creates a `GameObject`
2. Managed code adds a `SpriteRenderer` component
3. `SpriteRendererSystem` ensures a native sprite renderer exists
4. Native runtime stores a `SpriteRendererInstance`
5. During render, the runtime iterates native sprite renderer instances
6. Each instance delegates to backend-specific sprite render object implementation

## Concrete Type Layout

### Managed layer

Suggested files:

- `PieGameManaged/GameObject.cs`
- `PieGameManaged/Scene.cs`
- `PieGameManaged/Component.cs`
- `PieGameManaged/Transform.cs`
- `PieGameManaged/SpriteRenderer.cs`
- `PieGameManaged/SpriteRendererSystem.cs`

### Native runtime layer

Suggested files:

- `ApplicationDLL/Runtime/SpriteRendererInstance.h`
- `ApplicationDLL/Runtime/SpriteRendererInstance.cpp`
- `ApplicationDLL/Runtime/SpriteRendererRegistry.h`
- `ApplicationDLL/Runtime/SpriteRendererRegistry.cpp`

### Rendering layer

Suggested files:

- `ApplicationDLL/Renderer/ISpriteRenderObject.h`
- `ApplicationDLL/Renderer/SpriteRenderObject.cpp`
- `ApplicationDLL/SpriteRenderers/ISpriteRendererBackend.h`
- `ApplicationDLL/SpriteRenderers/Dx12SpriteRendererBackend.h`

If desired, `QuadRenderObject` can remain as the low-level geometry primitive because that is a rendering detail, not a gameplay concept.

## API Direction

### Current API

Current API is primitive-oriented:

- `CreateGameQuad`
- `DestroyGameQuad`
- `SetGameQuadTransform`

### Target API

Target API should be component-oriented:

- `CreateSpriteRenderer`
- `DestroySpriteRenderer`
- `SetSpriteRendererEnabled`
- `SetSpriteRendererTransform`
- `SetSpriteRendererTexture`
- `SetSpriteRendererMaterial`
- `SetSpriteRendererSortingLayer`
- `SetSpriteRendererOrder`

That matches Unity concepts more closely.

### Optional future API

If later moving more engine authority to native:

- `CreateGameObject`
- `DestroyGameObject`
- `SetParent`
- `AddSpriteRendererComponent`

Do not do this yet unless managed/native dual ownership is intentionally designed.

## Migration Strategy

Use incremental migration, not a big bang rewrite.

### Phase 1: Rename gameplay types in managed code

Safe rename only.

- `Entity` -> `GameObject`
- `GameScene` -> `Scene`
- `TransformComponent` -> `Transform`
- `SpriteRendererComponent` -> `SpriteRenderer`

No behavior change.

### Phase 2: Rename native sprite API

Replace `GameQuad` naming with sprite renderer naming.

- `IGameQuad` -> `ISpriteRenderObject`
- `CreateGameQuad` -> `CreateSpriteRenderer`
- `g_gameQuads` -> `g_spriteRenderers`

Still no major behavior change.

### Phase 3: Introduce `Component` base class

Move managed code closer to Unity.

- add `Component`
- make `SpriteRenderer : Component`
- make `Transform` accessible through `GameObject`

### Phase 4: Add `AddComponent<T>()`

Transition from typed fields to a component container.

Example target:

```csharp
var go = scene.CreateGameObject("Player");
var sprite = go.AddComponent<SpriteRenderer>();
sprite.Texture = "Assets/Texture/player.png";
```

### Phase 5: System-driven sync

Keep render synchronization in systems, not on `GameObject`.

- `SpriteRendererSystem`
- `TransformSyncSystem`

This preserves separation between gameplay model and native runtime.

## Recommended Final Naming

If the target is "Unity-like but not a Unity clone", this is the cleanest naming set.

### Managed

- `Scene`
- `GameObject`
- `Component`
- `Transform`
- `SpriteRenderer`

### Native runtime

- `SpriteRendererInstance`
- `SpriteRendererRegistry`

### Renderer internals

- `ISpriteRenderObject`
- `SpriteRenderObjectFactory`
- `Dx12SpriteRenderObject`
- `VulkanSpriteRenderObject`
- `OpenGlSpriteRenderObject`

This avoids leaking render primitive terminology into gameplay code.

## What To Avoid

Avoid these names:

- `GameQuadObject`
- `GameSpriteObject`
- `EntityComponentObject`

They combine multiple abstraction levels into one name.

## Practical Recommendation For This Repository

For this repository, the best next step is:

1. rename managed gameplay types to Unity names
2. rename native `GameQuad` API to `SpriteRenderer`
3. keep native ownership limited to render-component instances
4. do not add a native `GameObject` yet

That gives Unity-like authoring and naming without introducing dual scene ownership.

## Example End State

Managed usage:

```csharp
Scene scene = new Scene();
GameObject player = scene.CreateGameObject("Player");
SpriteRenderer spriteRenderer = player.AddComponent<SpriteRenderer>();
spriteRenderer.Texture = "Assets/Texture/textest.png";

player.Transform.CenterX = 0.0f;
player.Transform.CenterY = 0.0f;
player.Transform.Width = 1.0f;
player.Transform.Height = 1.0f;
```

Native implementation idea:

- `SpriteRendererSystem` maps each managed `SpriteRenderer` to a native `SpriteRendererInstance`
- `SpriteRendererInstance` owns an `ISpriteRenderObject`
- `ISpriteRenderObject` delegates to DX12/Vulkan/OpenGL implementation

That is close to Unity at the authoring level while keeping the renderer architecture clean.
