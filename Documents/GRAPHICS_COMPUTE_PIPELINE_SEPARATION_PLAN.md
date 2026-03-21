# Graphics / Compute を分離できる設計にしておくための手順書

## 1. このドキュメントの目的

このドキュメントは、改善計画の

- Step 9. Graphics / Compute を分離できる設計にしておく

を詳しく説明するためのものです。

ここでの目的は、今すぐ compute pipeline を実装することではありません。  
目的は、

**将来 compute shader を扱うときに、`PipelineLibrary` 周辺を大きく壊さず拡張できる形にしておくこと**

です。

## 2. まず結論

今の `PipelineLibrary` は実質的に

- Graphics Pipeline 専用

です。

これは現状としては正しいです。  
ただし将来 compute shader を扱いたくなったとき、

- `D3D12_GRAPHICS_PIPELINE_STATE_DESC`
- input layout
- rasterizer
- blend
- depth/stencil
- RTV format

といった graphics 固有要素が前提の設計だと、
compute を同じ型に無理やり押し込むことになります。

そのため Step 9 では、

- 今は graphics 専用のままでもよい
- ただし compute を混ぜても崩れない境界を先に作る

という整理が重要です。

## 3. 何が問題になるのか

## 3.1 graphics と compute は似ているようで違う

どちらも「パイプライン」と呼ばれますが、必要な情報はかなり違います。

### Graphics pipeline で必要なもの

- VS / PS
- input layout
- primitive topology
- rasterizer state
- blend state
- depth/stencil state
- render target format
- root signature

### Compute pipeline で必要なもの

- CS
- root signature

基本的に compute には次がありません。

- input layout
- rasterizer
- blend
- depth/stencil
- render target format

つまり、graphics と compute を同じ `Desc` に無理に詰めると、
不要なフィールドだらけになります。

## 3.2 今のまま compute を足すと起きやすい問題

たとえば今の `PipelineDesc` に無理やり compute を入れると、
次のような形になりがちです。

- `vertexShader` は空
- `pixelShader` は空
- `computeShader` だけ使う
- `inputElements` は無視
- `renderTargetFormat` は無視
- `blend` も無視

こうなると、

- どのフィールドが有効か分かりにくい
- `operator==` / hasher が複雑になる
- バグが入りやすい
- 読む人が混乱する

## 4. Step 9 の基本方針

Step 9 では、次の方針を採ります。

1. 今は graphics pipeline を主軸にする
2. compute 用の拡張余地だけ先に置く
3. まだ不要な複雑化は避ける

つまり、いきなり巨大な共通抽象を作るのではなく、

- graphics は graphics として明確化
- compute を入れる余白を残す

という方向が推奨です。

## 5. 目指す設計の考え方

考え方としては次の2案があります。

## 5.1 案A: まず graphics 専用を明確化する

これは実務上もっとも安全です。

### 形

- `GraphicsPipelineDesc`
- `GraphicsPipeline`
- `GraphicsPipelineCache`

を明示し、
今の `PipelineLibrary` は実質それだと分かるようにする

### 利点

- 現在のコードと一致している
- 無理な抽象化を避けられる
- compute 実装が必要になったときに別系統で足せる

### 欠点

- 最初は graphics / compute 共通 API がない

## 5.2 案B: 共通 `PipelineKind` を先に作る

これは拡張余地を先に入れる案です。

### 例

```cpp
enum class PipelineKind
{
    Graphics,
    Compute
};
```

そして desc に kind を持たせます。

```cpp
struct PipelineDesc
{
    PipelineKind kind = PipelineKind::Graphics;
    ...
};
```

### 利点

- 将来 compute 追加時に入口がある

### 欠点

- まだ compute を扱っていない段階では過剰抽象化になりやすい

## 5.3 推奨

現段階では **案A を基本** にしつつ、
最小限の `PipelineKind` だけ用意するのがちょうどよいです。

つまり、

- 名前と責務は graphics 専用に寄せる
- でも将来 compute を入れる入口は作っておく

という折衷案です。

## 6. 具体的に何を仕込んでおくべきか

## Step 9-1. `PipelineKind` を導入する

### 目的

今後 pipeline の種類が増える余地を型として作るためです。

### 例

```cpp
enum class PipelineKind : uint32_t
{
    Graphics = 0,
    Compute = 1,
};
```

### ポイント

- まだ compute 実装を入れなくてもよい
- kind を持つ場所を先に決めておく

## Step 9-2. desc を graphics / compute で分ける

### 目的

graphics と compute の必要項目を混ぜないためです。

### 推奨形

```cpp
struct GraphicsPipelineDesc
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
};

struct ComputePipelineDesc
{
    ShaderCache::ShaderProgramDesc computeShader;
    RootSignatureCache::RootSignatureDesc rootSignature;
};
```

### 重要

この時点では `ComputePipelineDesc` を定義だけして、
まだ使わなくても構いません。

## Step 9-3. 現行 `PipelineLibrary` の責務を graphics に限定して明示する

### 目的

今のクラスが何を扱うのかを誤解させないためです。

### やること

- コメントで `PipelineLibrary` は graphics PSO 用であると明示する
- 可能なら内部命名を `graphicsPipelineDesc` に寄せる
- 将来 rename しやすい構造にする

### 例

- `CreateGraphicsPipeline(...)`
- `GraphicsPipelineDesc`
- `GraphicsPipelineDescHasher`

## Step 9-4. compute 用の生成入口だけ作っておく

### 目的

将来 compute 実装を足す際に、無理な変更を減らします。

### 例

```cpp
HRESULT GetOrCreateGraphics(
    ID3D12Device* device,
    const GraphicsPipelineDesc& desc,
    std::shared_ptr<const GraphicsPipeline>* outPipeline);

HRESULT GetOrCreateCompute(
    ID3D12Device* device,
    const ComputePipelineDesc& desc,
    std::shared_ptr<const ComputePipeline>* outPipeline);
```

### 現時点の推奨

`GetOrCreateCompute()` は宣言だけでも構いません。  
未実装なら後回しにしてよいです。

## 7. 今すぐやるべき最小対応

今の段階で無理なくやるなら、次の3点で十分です。

1. `PipelineKind` を導入する
2. `GraphicsPipelineDesc` という名前を導入する
3. `PipelineLibrary` は graphics PSO であると明示する

これだけでも、将来 compute を入れるときの見通しがかなり良くなります。

## 8. 今やらなくてよいこと

Step 9 でやりがちな過剰対応があります。

### 8.1 giant union 的な共通 desc

たとえば、

- VS
- PS
- CS
- input layout
- blend
- depth
- compute 用専用設定

を全部1つの `PipelineDesc` に入れる設計です。

これは避けた方がよいです。

### 理由

- 今の段階では複雑すぎる
- どの項目が有効か分かりにくい
- compare / hash が壊れやすい

### 8.2 抽象インターフェースを先に作りすぎる

たとえば、

- `IPipeline`
- `IPipelineCache`
- `IPipelineDesc`

のような抽象をいきなり作るのは早すぎます。

今は DX12 固定なので、まずは具体クラスで十分です。

## 9. 比較・ハッシュ設計での注意

graphics と compute を分けるなら、
比較・ハッシュも分けるのが自然です。

### graphics 側

ハッシュ対象:

- VS / PS
- root signature
- input layout
- blend / depth / rasterizer
- RTV format

### compute 側

ハッシュ対象:

- CS
- root signature

この差があるので、1つの hasher に無理にまとめるより、
型自体を分けた方が安全です。

## 10. 将来的な完成形イメージ

最終的には次のような構成が理想です。

- `ShaderCache`
  - shader blob を返す
- `RootSignatureCache`
  - root signature を返す
- `GraphicsPipelineCache`
  - graphics PSO を返す
- `ComputePipelineCache`
  - compute PSO を返す

このとき graphics と compute は、

- 共通概念としては「pipeline」
- 実装上は別系統

になっている方が保守しやすいです。

## 11. 実施順の推奨

### Phase 1

- `PipelineKind` 導入
- `GraphicsPipelineDesc` 命名導入

### Phase 2

- `PipelineLibrary` を graphics 専用として整理
- graphics 固有コメント・関数名を見直す

### Phase 3

- `ComputePipelineDesc` を追加
- 将来 compute 用 API の入口を追加

### Phase 4

- 必要になった段階で `ComputePipelineCache` を実装

## 12. テスト観点

このステップはまだ compute 実装ではないため、
主なテスト観点は回帰確認です。

### graphics 側で確認すること

- 既存の quad 描画が壊れないか
- `Material::Initialize()` が従来通り動くか
- PSO キャッシュヒットが維持されるか

### 設計確認

- graphics 専用コードと compute 用余地が混ざりすぎていないか
- 不要なフィールドが desc に入っていないか

## 13. まとめ

Step 9 の本質は、

**「まだ compute を作らないが、compute を入れた瞬間に破綻しない構造へしておくこと」**

です。

最重要ポイントは次の通りです。

- graphics と compute の必要情報は違う
- だから desc も cache も分けた方がよい
- ただし今すぐ全部実装する必要はない

現段階では、

- `PipelineKind`
- `GraphicsPipelineDesc`
- graphics 専用責務の明示

までやっておけば十分です。

必要なら次に、これをベースに今の `PipelineLibrary.h` をどう切り直すかの具体案まで展開できます。

## 14. `PipelineLibrary.h` をどう切り直すかの具体案

この章では、現在の `PipelineLibrary.h` を

- graphics 専用責務を明確化しつつ
- compute を将来足せる形に寄せる

ための、具体的な切り直し方を示します。

## 14.1 現在の問題意識

今の `PipelineLibrary.h` は、実態として graphics PSO 用であるにもかかわらず、
名前や構造がやや汎用的です。

そのため、将来 compute を入れたくなったときに、

- 既存 `PipelineDesc` に無理やり `computeShader` を追加する
- graphics 用メンバを compute で無視する

といった崩れ方をしやすくなります。

これを避けるには、

- まず graphics 側の型を明確にする
- その上で compute 用の余地を別型で作る

という順番が必要です。

## 14.2 今の `PipelineLibrary.h` に対する基本方針

まずは次の方針で整理するのが安全です。

1. `InputElementDesc` は graphics 専用型として残す
2. `PipelineDesc` を `GraphicsPipelineDesc` 相当に明示化する
3. `Pipeline` も `GraphicsPipeline` 相当に意味づけする
4. `GetOrCreate()` は graphics 用であることが分かる形に寄せる
5. compute 用の型は宣言だけ先に置いてもよい

## 14.3 最小変更での切り直し案

一気に rename しすぎると差分が大きくなるので、
最初は「中身の意味を先に正す」方が安全です。

### 変更イメージ

```cpp
class PipelineLibrary final
{
public:
    enum class PipelineKind : uint32_t
    {
        Graphics = 0,
        Compute = 1,
    };

    struct InputElementDesc
    {
        std::string semanticName;
        UINT semanticIndex = 0;
        DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;
        UINT inputSlot = 0;
        UINT alignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
        D3D12_INPUT_CLASSIFICATION inputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
        UINT instanceDataStepRate = 0;

        bool operator==(const InputElementDesc& other) const;
    };

    struct GraphicsPipelineDesc
    {
        PipelineKind kind = PipelineKind::Graphics;

        ShaderCache::ShaderProgramDesc vertexShader;
        ShaderCache::ShaderProgramDesc pixelShader;
        RootSignatureCache::RootSignatureDesc rootSignature;

        DXGI_FORMAT renderTargetFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
        D3D12_CULL_MODE cullMode = D3D12_CULL_MODE_NONE;
        D3D12_PRIMITIVE_TOPOLOGY_TYPE topologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        bool enableDepth = false;
        bool enableBlend = false;
        std::vector<InputElementDesc> inputElements;

        bool operator==(const GraphicsPipelineDesc& other) const;
    };

    struct GraphicsPipeline
    {
        Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState;
        Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature;
    };

    HRESULT GetOrCreateGraphics(
        ID3D12Device* device,
        const GraphicsPipelineDesc& desc,
        std::shared_ptr<const GraphicsPipeline>* outPipeline);

    void Clear();
};
```

### この案のポイント

- `PipelineKind` は先に置く
- graphics 用 desc を別名で明示する
- `rootSignature` は単数で持つ
- `GetOrCreateGraphics()` で graphics 専用だと明示する

## 14.4 互換性を意識した段階移行案

既存呼び出し側への影響を抑えるなら、いきなり全面 rename せず、
次のような段階で移行できます。

### Phase A. 新型を追加する

まず `PipelineLibrary.h` に次を追加します。

- `PipelineKind`
- `GraphicsPipelineDesc`
- `GraphicsPipeline`

この段階では、既存の `PipelineDesc` と `Pipeline` を残しても構いません。

### Phase B. 型エイリアスでつなぐ

差分を小さくしたいなら、一時的に次のような橋渡しもできます。

```cpp
using PipelineDesc = GraphicsPipelineDesc;
using Pipeline = GraphicsPipeline;
```

こうすると、実装を大きく壊さずに意味だけ先に正せます。

### Phase C. API 名を明確化する

次にメソッド名を graphics 専用へ寄せます。

```cpp
HRESULT GetOrCreateGraphics(...);
```

必要なら移行期間中だけ、

```cpp
HRESULT GetOrCreate(...) { return GetOrCreateGraphics(...); }
```

のような互換 wrapper を残してもよいです。

### Phase D. 旧 generic 名を削除する

呼び出し側の置換が済んだら、次を整理します。

- 旧 `PipelineDesc`
- 旧 `Pipeline`
- 旧 `GetOrCreate`

## 14.5 compute 用の余白をどう置くか

この段階では compute 実装を入れなくても、
次の型だけ先に置くことができます。

```cpp
struct ComputePipelineDesc
{
    PipelineKind kind = PipelineKind::Compute;

    ShaderCache::ShaderProgramDesc computeShader;
    RootSignatureCache::RootSignatureDesc rootSignature;

    bool operator==(const ComputePipelineDesc& other) const;
};

struct ComputePipeline
{
    Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState;
    Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature;
};
```

ただしこの時点では、

- 宣言だけ
- 実装は未着手

で十分です。

## 14.6 `InputElementDesc` はどこに置くべきか

`InputElementDesc` は graphics 専用の概念です。  
したがって、最終的には次のどちらかに寄せると分かりやすくなります。

### 案1

`PipelineLibrary` の中に残しつつ、
`GraphicsPipelineDesc` の内部専用として使う

### 案2

`GraphicsPipelineDesc::InputElementDesc` としてネストする

例:

```cpp
struct GraphicsPipelineDesc
{
    struct InputElementDesc
    {
        ...
    };

    std::vector<InputElementDesc> inputElements;
};
```

今の段階では案1で十分です。  
まずは用途を graphics 側に固定することの方が重要です。

## 14.7 hasher をどう分けるか

graphics / compute 分離に合わせて hasher も分けるべきです。

### graphics 側

```cpp
struct GraphicsPipelineDescHasher
{
    size_t operator()(const GraphicsPipelineDesc& desc) const;
};
```

### compute 側

```cpp
struct ComputePipelineDescHasher
{
    size_t operator()(const ComputePipelineDesc& desc) const;
};
```

こうしておくと、

- graphics 側は input layout や RTV format を含む
- compute 側は compute shader と root signature だけを見る

という自然な分離ができます。

## 14.8 private メソッドも graphics 名へ寄せる

ヘッダを切り直すなら、private メソッドも意味をそろえた方がよいです。

たとえば今の

- `CreatePipeline(...)`

は、将来的には次のように寄せられます。

```cpp
HRESULT CreateGraphicsPipeline(
    ID3D12Device* device,
    const GraphicsPipelineDesc& desc,
    std::shared_ptr<const GraphicsPipeline>* outPipeline) const;
```

これで「このクラスは graphics PSO を作る」という意図が伝わりやすくなります。

## 14.9 今すぐ無理に変えなくてよい部分

次は後回しでも構いません。

- クラス名を `GraphicsPipelineCache` に rename
- compute 用 API の実装
- 抽象基底クラス化

Step 9 の段階では、
ヘッダの意味を正しておくことが主目的です。

## 14.10 推奨する実際の進め方

最も安全な進め方は次です。

1. `PipelineKind` を追加
2. `GraphicsPipelineDesc` / `GraphicsPipeline` を追加
3. `using PipelineDesc = GraphicsPipelineDesc;`
   `using Pipeline = GraphicsPipeline;`
   の形で一時互換を取る
4. `GetOrCreateGraphics()` を追加
5. 旧 `GetOrCreate()` から委譲させる
6. 呼び出し側置換後に旧名を削除

この順なら、ヘッダの意味を改善しつつ差分を制御できます。
