# RootSignatureCache 整理手順書

## 1. 目的

このドキュメントは、`Step 6. RootSignatureCache を分離する` を進める途中で、
現在の実装が混線しやすいポイントを整理し、
**どういう順番で責務を切り出していけば安全か**
をまとめたものです。

対象は主に次のファイルです。

- `ApplicationDLL/Renderer/RootSignatureCache.h`
- `ApplicationDLL/Renderer/RootSignatureCache.cpp`
- `ApplicationDLL/Renderer/PipelineLibrary.h`
- `ApplicationDLL/Renderer/PipelineLibrary.cpp`

## 2. まず押さえるべきこと

### 2.1 Root Signature は「1個の中に複数 parameter を持つ」

現在の混乱の原因はここです。

DirectX 12 の Root Signature は、

- Root Parameter が複数入る
- Static Sampler も複数入る

という構造です。

つまり、

- `DescriptorTableSrv` が1個
- `CBV` が1個
- `Sampler` が1個

あったとしても、それは **Root Signature が3個ある** のではなく、
**1個の Root Signature の中に3つの要素がある**
という意味です。

この前提が崩れると、型設計が全部崩れます。

### 2.2 分けるべき単位

整理後の正しい単位は次です。

- `RootParameterDesc`
  - Root Parameter 1個分
- `StaticSamplerDesc`
  - Static Sampler 1個分
- `RootSignatureDesc`
  - Root Parameter 群 + Static Sampler 群 + flags をまとめたもの

そして `RootSignatureCache` は、

- `RootSignatureDesc` をキーにして
- `ID3D12RootSignature` を返す

クラスであるべきです。

## 3. 現在起きやすい問題

途中実装では、次の混線が起きやすいです。

- `RootSignatureDesc` が単一 parameter のような形になっている
- なのに実装では `rootParameters` や `staticSamplers` を持つ前提で使っている
- `PipelineLibrary` 側で root signature 生成コードが残っている
- `RootSignatureCache` のキャッシュ値が blob なのか root signature なのか曖昧になっている
- `PipelineDesc` 側で root signature 全体と root parameter 群の区別が崩れている

この状態では、型をいじるたびに連鎖的に壊れます。

## 4. 整理の基本方針

進め方は次の原則に従います。

1. 先に型を正す
2. 次に `RootSignatureCache` 単体を完成させる
3. 最後に `PipelineLibrary` から旧コードを外す

これを逆順にすると、壊れた責務がさらに混ざります。

## 5. 最終的な責務分担

整理後の責務は次のようにするのが自然です。

### ShaderCache

- ShaderDesc をキーにする
- `ID3DBlob` を返す

### RootSignatureCache

- `RootSignatureDesc` をキーにする
- `ID3D12RootSignature` を返す

### PipelineLibrary

- Shader blob を受け取る
- Root Signature を受け取る
- Graphics PSO を作る
- PSO をキャッシュする

ここで重要なのは、

- ShaderCache は shader まで
- RootSignatureCache は root signature まで
- PipelineLibrary は PSO まで

と責務を止めることです。

## 6. 整理手順

## Step 1. 型を作り直す

### 目的

まずは「何を1個と数えるのか」を型で固定します。

### やること

`RootSignatureCache.h` に、少なくとも次の3種類を持たせます。

```cpp
struct RootParameterDesc
{
    enum class Type : unsigned int
    {
        DescriptorTableSrv,
        ConstantBufferView
    };

    Type type = Type::DescriptorTableSrv;
    D3D12_SHADER_VISIBILITY shaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

    UINT numDescriptors = 1;
    UINT baseShaderRegister = 0;
    UINT registerSpace = 0;

    UINT cbvShaderRegister = 0;
    UINT cbvRegisterSpace = 0;

    bool operator==(const RootParameterDesc& other) const;
};

struct StaticSamplerDesc
{
    D3D12_FILTER filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
    D3D12_TEXTURE_ADDRESS_MODE addressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    D3D12_TEXTURE_ADDRESS_MODE addressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    D3D12_TEXTURE_ADDRESS_MODE addressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    UINT shaderRegister = 0;
    UINT registerSpace = 0;
    D3D12_SHADER_VISIBILITY shaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    D3D12_COMPARISON_FUNC comparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
    D3D12_STATIC_BORDER_COLOR borderColor = D3D12_STATIC_BORDER_COLOR_OPAQUE_WHITE;
    FLOAT mipLODBias = 0.0f;
    UINT maxAnisotropy = 1;
    FLOAT minLOD = 0.0f;
    FLOAT maxLOD = D3D12_FLOAT32_MAX;

    bool operator==(const StaticSamplerDesc& other) const;
};

struct RootSignatureDesc
{
    D3D12_ROOT_SIGNATURE_FLAGS flags =
        D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

    std::vector<RootParameterDesc> rootParameters;
    std::vector<StaticSamplerDesc> staticSamplers;

    bool operator==(const RootSignatureDesc& other) const;
};
```

### ポイント

- `RootSignatureDesc` は単一 parameter ではない
- 「Root Signature 全体」を表す集約型にする

## Step 2. hasher を作る

### 目的

`unordered_map` のキーとして使えるようにするためです。

### やること

- `RootParameterDescHasher`
- `StaticSamplerDescHasher`
- `RootSignatureDescHasher`

を作るか、
最低でも `RootSignatureDescHasher` の中で全部のメンバを hash combine します。

### 注意

- `rootParameters` の順序は意味を持つ
- `staticSamplers` の順序も変えない
- ソートして正規化しない

順序を変えると別の Root Signature です。

## Step 3. RootSignatureCache の公開 API を決める

### 推奨 API

```cpp
class RootSignatureCache final
{
public:
    HRESULT GetOrCreate(
        ID3D12Device* device,
        const RootSignatureDesc& desc,
        Microsoft::WRL::ComPtr<ID3D12RootSignature>* outRootSignature);

    void Clear();

private:
    std::mutex mutex_;
    std::unordered_map<
        RootSignatureDesc,
        Microsoft::WRL::ComPtr<ID3D12RootSignature>,
        RootSignatureDescHasher> cache_;
};
```

### 理由

- `HRESULT` にすると DX12 の他コードと揃う
- `ID3D12Device*` を毎回受け取るとデバイス境界が明確
- 値型は `ComPtr<ID3D12RootSignature>` に固定する

### やってはいけないこと

- 値型を `ID3DBlob` にする
- `bool` を返して失敗理由を失う
- `Pipeline` 型を持ち込む

## Step 4. RootSignatureCache を単体で完成させる

### 目的

`PipelineLibrary` に戻す前に、まず root signature 生成だけを独立して成立させます。

### `GetOrCreate()` の中でやること

1. 引数チェック
2. キャッシュ検索
3. なければ DX12 の一時構造へ変換
4. `D3D12SerializeRootSignature()`
5. `device->CreateRootSignature()`
6. キャッシュ保存
7. 返却

### この段階でやらないこと

- PSO 作成
- Shader blob の扱い
- Material との結合

## Step 5. 実装を helper に分ける

### 目的

`GetOrCreate()` に全部書くと読みづらく、寿命管理も崩れやすいです。

### 推奨 helper

- `BuildRootParameters(...)`
- `BuildStaticSamplers(...)`
- `CreateRootSignatureInternal(...)`

### 例

```cpp
HRESULT CreateRootSignatureInternal(
    ID3D12Device* device,
    const RootSignatureDesc& desc,
    Microsoft::WRL::ComPtr<ID3D12RootSignature>* outRootSignature);
```

### 注意

`D3D12_ROOT_PARAMETER` の descriptor table は、
`D3D12_DESCRIPTOR_RANGE*` を内部で参照します。

そのため、

- `descriptorRanges`
- `rootParameters`
- `staticSamplers`

は `D3D12SerializeRootSignature()` が終わるまで同じスコープで生きている必要があります。

## Step 6. 先に PipelineLibrary 内の旧コードを helper 化する

### 目的

いきなり `RootSignatureCache` に移すと差分が大きくなります。  
まず `PipelineLibrary.cpp` の root signature 生成部を helper に抜きます。

### やること

現在 `PipelineLibrary::CreatePipeline()` にある以下を一旦 helper にまとめます。

- `D3D12_ROOT_SIGNATURE_DESC` 構築
- root parameter 変換
- static sampler 変換
- serialize
- `CreateRootSignature`

たとえば次のような private helper にします。

```cpp
HRESULT CreateRootSignatureFromDesc(
    ID3D12Device* device,
    const RootSignatureDesc& desc,
    Microsoft::WRL::ComPtr<ID3D12RootSignature>* outRootSignature);
```

### 理由

この helper が動いた状態になれば、
次はそれをクラスごと `RootSignatureCache` へ移すだけになります。

## Step 7. PipelineDesc の持ち方を直す

### 目的

ここが一番壊れやすい箇所です。

### 今の誤りの典型

- `std::vector<RootSignatureDesc> rootParameters`

これは意味が崩れています。

### 正しい形

`PipelineDesc` には、root signature 全体を単数で持たせます。

```cpp
struct PipelineDesc
{
    ShaderCache::ShaderProgramDesc vertexShader;
    ShaderCache::ShaderProgramDesc pixelShader;

    RootSignatureCache::RootSignatureDesc rootSignature;

    DXGI_FORMAT renderTargetFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
    D3D12_CULL_MODE cullMode = D3D12_CULL_MODE_NONE;
    D3D12_PRIMITIVE_TOPOLOGY_TYPE topologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    bool enableDepth = false;
    bool enableBlend = false;
    std::vector<InputElementDesc> inputElements;

    bool operator==(const PipelineDesc& other) const;
};
```

### 理由

- `rootParameters` は root signature の構成要素
- `PipelineDesc` が持つべきなのは root signature そのもの

## Step 8. PipelineLibrary から旧 root signature 生成コードを削除する

### 目的

責務分離を完了させます。

### 変更後の流れ

`PipelineLibrary::CreatePipeline()` ではこうします。

1. VS blob を `ShaderCache` から取得
2. PS blob を `ShaderCache` から取得
3. Root Signature を `RootSignatureCache` から取得
4. 取得した root signature を `pipelineDesc.pRootSignature` に設定
5. `CreateGraphicsPipelineState()`

### この段階で消すもの

- root parameter 変換コード
- static sampler 変換コード
- `D3D12SerializeRootSignature()`
- `CreateRootSignature()` 直呼び

## Step 9. キャッシュの所有場所を決める

### 選択肢 A. PipelineLibrary のメンバとして持つ

利点:

- 変更量が少ない
- 今の構造に入れやすい

弱点:

- 将来他の場所から再利用しにくい

### 選択肢 B. ランタイム共有オブジェクトとして持つ

利点:

- デバイス単位で共有しやすい
- 責務がはっきりする

弱点:

- 導入差分が増える

### 推奨

最初は **A** で十分です。  
まず動かしてから共有化を考える方が安全です。

## Step 10. 最後に API と命名を整える

### 推奨命名

- `RootParameterDesc`
- `StaticSamplerDesc`
- `RootSignatureDesc`
- `RootSignatureDescHasher`
- `RootSignatureCache::GetOrCreate(...)`

### 避けたい命名

- `RootSignatureType` にすべて詰める
- `RootSignatureDesc` なのに parameter 1個しか持たない
- `compiledBlob` のような shader 用語を root signature 側で使う

## 7. 実装時の注意点

### 7.1 値型は root signature にする

キャッシュの値は必ずこれです。

```cpp
Microsoft::WRL::ComPtr<ID3D12RootSignature>
```

`ID3DBlob` ではありません。

### 7.2 `D3D12SerializeRootSignature()` に渡すのは DX12 の構造体

渡すのは自作 `RootSignatureDesc` ではなく、
変換後の `D3D12_ROOT_SIGNATURE_DESC` です。

### 7.3 `Pipeline` 型を持ち込まない

`RootSignatureCache` は PSO を知らなくてよいです。  
root signature 単体だけを返すべきです。

### 7.4 `ID3D12Device*` を引数に含める

デバイス境界を意識するためです。  
静的グローバルで完全固定にすると将来拡張で詰まりやすいです。

### 7.5 順序を変えない

- root parameter の順番
- static sampler の順番

はそのまま比較・hash に使います。

## 8. 推奨実施順

最小リスクで進めるなら次の順です。

1. `RootParameterDesc` / `StaticSamplerDesc` / `RootSignatureDesc` を定義
2. `operator==` と hasher を整備
3. `RootSignatureCache::GetOrCreate()` を単体完成
4. `PipelineLibrary` 内 root signature 生成部を helper 化
5. `PipelineDesc` を `rootSignature` 単数保持に変更
6. `PipelineLibrary` から `RootSignatureCache` を呼ぶよう変更
7. 旧 root signature 生成コードを削除

## 9. 完成形のイメージ

最終的にコード上の責務はこう見える状態が理想です。

- `ShaderCache`
  - shader bytecode を返す
- `RootSignatureCache`
  - root signature を返す
- `PipelineLibrary`
  - PSO を返す

この状態になると、
DX12 の概念単位で責務が分かれ、
問題発生時も切り分けがしやすくなります。

## 10. まとめ

`RootSignatureCache` 整理で一番重要なのは、

**「Root Signature 全体」と「その中の parameter 1個」を絶対に混同しないこと**

です。

その前提を型で固定し、

1. 型を正す
2. cache 単体を完成させる
3. PipelineLibrary から旧コードを外す

の順で進めれば、無理なく分離できます。
