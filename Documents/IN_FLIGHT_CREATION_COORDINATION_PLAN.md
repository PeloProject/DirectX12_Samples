# in-flight 生成競合を抑止する対応手順書

## 1. このドキュメントの目的

このドキュメントは、改善計画の

- Step 8. in-flight 生成競合を抑止する

を詳しく説明するためのものです。

対象は主に次のようなキャッシュ系クラスです。

- `ShaderCache`
- `RootSignatureCache`
- `PipelineLibrary` または `GraphicsPsoCache`

ここでいう `in-flight` とは、

**「まだ生成中で、キャッシュには未確定だが、すでに誰かが作り始めている状態」**

を意味します。

## 2. まず結論

今のようなキャッシュ実装でよくある流れは次です。

1. lock を取る
2. cache を探す
3. 見つからなければ lock を外す
4. 重い生成処理を行う
5. もう一度 lock を取って cache に入れる

この方式は単純でよく使われますが、
**複数スレッドが同じキーを同時に要求したとき、同じものを二重生成する**
可能性があります。

これを防ぐのが in-flight 制御です。

要点は次の1文です。

**「生成済み」だけでなく「生成中」も管理する**

ということです。

## 3. 何が問題なのか

## 3.1 典型的な競合シナリオ

たとえば、2つのスレッド A と B が同じ shader を要求したとします。

### 現在の素朴な流れ

1. A が cache を見る
2. A は見つからない
3. A は lock を外して compile を始める
4. B が cache を見る
5. B も見つからない
6. B も lock を外して compile を始める
7. A が先に完成して cache に入れる
8. B も完成して cache に入れようとする

結果:

- 同じ shader を2回コンパイルしている
- 最終的には cache は1個でも、無駄な処理をした

## 3.2 なぜ困るのか

この二重生成は、次のような問題を起こします。

- 無駄に CPU 時間を使う
- 起動時間やロード時間が伸びる
- ログが読みにくくなる
- 重い生成処理だと目に見えて遅くなる
- 将来の大規模アセットロードで効率が落ちる

Shader compile や PSO 作成は軽くないので、
並列化するならなおさら抑止したい種類の処理です。

## 4. in-flight 制御とは何か

`in-flight` 制御とは、キャッシュに対して

- 生成済み
- 未生成

だけでなく、

- 生成中

の状態も持たせる考え方です。

これにより、

- 先に作り始めたスレッド
  - 実際に生成を担当
- 後から来たスレッド
  - その生成完了を待つ

という分担にできます。

## 5. どのクラスに必要か

### 5.1 優先度が高い

- `ShaderCache`
- `RootSignatureCache`
- `PipelineLibrary`

### 5.2 特に効果が高い順

一般には次の順で効果が大きいです。

1. `ShaderCache`
2. `PipelineLibrary`
3. `RootSignatureCache`

理由:

- shader compile は比較的重い
- PSO 作成も重い
- root signature 生成は相対的に軽いが、整理としては同じ発想で揃えられる

## 6. いつ対応すべきか

### 6.1 まだ不要なケース

次の条件なら、今すぐ入れなくてもよいです。

- シングルスレッドしか使っていない
- 起動時に順番にしか生成しない
- 実害がまだ出ていない

### 6.2 入れる価値が高いケース

次の条件なら優先度が上がります。

- 複数スレッドでアセットロードしている
- shader / PSO 作成がボトルネックになっている
- 同じキーへの要求が並列に飛ぶ可能性がある

## 7. 実装方式の候補

in-flight 制御にはいくつか方式があります。

## 7.1 方式A: 生成中フラグ付きエントリ

### イメージ

キャッシュの値を単純なオブジェクトではなく、
状態付きのエントリにします。

例:

```cpp
enum class EntryState
{
    InFlight,
    Ready,
    Failed
};

template<typename TValue>
struct CacheEntry
{
    EntryState state = EntryState::InFlight;
    TValue value;
    HRESULT hr = E_FAIL;
    std::condition_variable cv;
};
```

### 流れ

1. key を検索
2. なければ `InFlight` エントリを入れる
3. そのスレッドが生成担当になる
4. 後続スレッドは同じ key を見つけたら待機する
5. 完成したら `Ready` にして通知する

### 利点

- C++ 標準機能だけで素直に実装できる
- 状態遷移が見やすい

### 欠点

- `condition_variable` と mutex の扱いを正しく書く必要がある

## 7.2 方式B: `std::shared_future` を使う

### イメージ

生成結果を future として共有します。

```cpp
std::unordered_map<Key, std::shared_future<Result>, Hasher> inFlight_;
```

### 流れ

1. 生成中マップに future を登録
2. 後続はその future を待つ
3. 完成したら結果を受け取る

### 利点

- 待機処理が書きやすい
- 非同期タスクとの相性がよい

### 欠点

- 既存コードベースにとっては少し抽象度が上がる
- `HRESULT + ComPtr` の扱いをラップする必要がある

## 7.3 方式C: 二重生成を許容したまま最終挿入だけ競合回避

### イメージ

今の方式のまま、二重生成自体は許容します。

### 利点

- 実装が簡単

### 欠点

- Step 8 の目的を満たさない

### 結論

これは「対応しない」のに近いので、Step 8 の解決策にはなりません。

## 8. 推奨方式

このコードベースでは、最初は **方式A: 生成中フラグ付きエントリ** が推奨です。

理由:

- 既存コードが `HRESULT + out parameter` ベース
- `ComPtr` をそのまま持ちやすい
- `std::future` を導入せずに済む
- ロジックを追いやすい

## 9. 実装イメージ

## 9.1 エントリ構造

たとえば shader cache なら次のようなエントリが考えられます。

```cpp
struct ShaderCacheEntry
{
    bool inFlight = true;
    bool ready = false;
    HRESULT hr = E_FAIL;
    Microsoft::WRL::ComPtr<ID3DBlob> blob;
    std::condition_variable cv;
};
```

実際には `inFlight` と `ready` を enum 1つにまとめても構いません。

## 9.2 概念的な流れ

### 生成担当スレッド

1. lock を取る
2. key が無ければ `InFlight` エントリを挿入
3. lock を外す
4. 実際に compile / create を行う
5. lock を取る
6. 結果を書き込む
7. `Ready` または `Failed` にする
8. notify する

### 後続スレッド

1. lock を取る
2. key がある
3. それが `InFlight` なら wait する
4. `Ready` になったら結果を受け取る
5. `Failed` ならその失敗を受け取る

## 10. ShaderCache に入れる場合の詳細

## 10.1 目的

同じ shader compile を並列に二重で走らせないことです。

### 効果

- 同じ shader file + entry point + model + flags の compile を1回にできる
- 起動時・ロード時の無駄が減る

### キー

- `ShaderDesc`

### 値

- `ID3DBlob`

## 10.2 エラー扱い

shader compile が失敗した場合、
待っていた他スレッドにも同じ失敗を返すべきです。

そのため、in-flight エントリは次を持つ必要があります。

- `HRESULT`
- `ID3DBlob`
- `state`

## 11. RootSignatureCache に入れる場合の詳細

## 11.1 目的

同じ root signature を並列に二重生成しないことです。

### キー

- `RootSignatureDesc`

### 値

- `ID3D12RootSignature`

### 注意

`RootSignatureCache` は shader compile より軽い可能性がありますが、
設計を揃える意味で同じ手法を使うと理解しやすくなります。

## 12. PipelineLibrary に入れる場合の詳細

## 12.1 目的

同じ PSO を並列に二重生成しないことです。

### キー

- `PipelineDesc`

### 値

- `Pipeline` または `ComPtr<ID3D12PipelineState>`

### 注意

PSO 生成は shader blob と root signature に依存するため、
下位の cache が in-flight 対応済みなら、ここも一貫して揃えた方が自然です。

## 13. 実装時の注意点

## 13.1 lock を保持したまま重い処理をしない

これは最重要です。

悪い例:

- mutex を握ったまま shader compile
- mutex を握ったまま `CreateGraphicsPipelineState`

これをすると、他のすべての要求が詰まります。

正しい流れは、

- lock で状態を登録
- lock を外す
- 重い処理
- lock で結果を書き戻す

です。

## 13.2 待機中にスプリアスウェイクアップを考慮する

`condition_variable` を使う場合は、
`if` ではなく `while` か predicate 付き `wait` を使います。

例:

```cpp
entry.cv.wait(lock, [&] { return entry.state != EntryState::InFlight; });
```

## 13.3 失敗時も必ず通知する

生成失敗時に notify し忘れると、
待っているスレッドが永久にブロックする危険があります。

成功時だけでなく失敗時も、

- 状態更新
- notify

を行う必要があります。

## 13.4 生成失敗をキャッシュするか決める

ここは設計判断が必要です。

### 失敗も保持する場合

利点:

- 同じ壊れた要求を何度も重複実行しない

欠点:

- 一時的な失敗でも固定化される

### 失敗を保持しない場合

利点:

- 次回再試行できる

欠点:

- 同じ失敗を繰り返す可能性がある

### 推奨

初期版では、

- in-flight 中の失敗は待っていた全員に返す
- その後エントリを消す

が扱いやすいです。

## 13.5 static グローバルと組み合わせる場合は注意

cache が static グローバルだと、

- デバイス境界
- shutdown 順序
- 再初期化時の古いエントリ

の問題が起きやすくなります。

可能なら cache はインスタンス所有の方が安全です。

## 14. 導入手順

## Step 8-1. まず ShaderCache で試す

理由:

- もっとも効果が分かりやすい
- ロジックが単純
- 不具合時の切り分けがしやすい

### やること

- `ShaderCacheEntry` を導入
- `cache_` の値型を entry に変更
- `GetOrCreate()` を in-flight 対応に書き換える

## Step 8-2. RootSignatureCache に同じ方式を適用する

### やること

- `RootSignatureCacheEntry` を導入
- `GetOrCreate()` を in-flight 対応にする

## Step 8-3. PipelineLibrary に適用する

### やること

- `Pipeline` 用 entry を導入
- `GetOrCreate()` を in-flight 対応にする

## Step 8-4. 統計ログを追加する

次の値を記録すると効果が見えやすくなります。

- cache hit 数
- cache miss 数
- in-flight wait 数
- create failure 数

## 15. テスト観点

## 15.1 正常系

- 同じキーを単発で要求したときに従来通り動くか
- 同じキーを並列要求したときに生成が1回で済むか

## 15.2 失敗系

- compile / create が失敗したとき、待機側が正しく解除されるか
- deadlock しないか

## 15.3 ログ確認

- `inFlightWaitCount` が増える状況で、生成回数が増えすぎていないか
- 同一キーで二重生成されていないか

## 16. 実装の擬似コード

概念的には次のような流れです。

```cpp
HRESULT GetOrCreate(const Key& key, Value* outValue)
{
    std::shared_ptr<Entry> entry;
    bool isCreator = false;

    {
        std::unique_lock<std::mutex> lock(mutex_);
        auto it = cache_.find(key);
        if (it == cache_.end())
        {
            entry = std::make_shared<Entry>();
            entry->state = EntryState::InFlight;
            cache_[key] = entry;
            isCreator = true;
        }
        else
        {
            entry = it->second;
            if (entry->state == EntryState::InFlight)
            {
                entry->cv.wait(lock, [&] { return entry->state != EntryState::InFlight; });
            }
        }
    }

    if (isCreator)
    {
        Value createdValue;
        HRESULT hr = CreateValue(key, &createdValue);

        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (SUCCEEDED(hr))
            {
                entry->value = createdValue;
                entry->state = EntryState::Ready;
                entry->hr = hr;
            }
            else
            {
                entry->state = EntryState::Failed;
                entry->hr = hr;
            }
        }

        entry->cv.notify_all();
    }

    if (FAILED(entry->hr))
    {
        return entry->hr;
    }

    *outValue = entry->value;
    return S_OK;
}
```

実装時はこの骨格をベースに、各 cache に合わせて調整すると分かりやすいです。

## 17. まとめ

Step 8 の本質は、

**「未生成」と「生成中」を区別すること**

です。

キャッシュは通常、

- ある
- ない

だけを見がちですが、並列化を考えると

- いま誰かが作っている

という状態を持たないと、同じものを何度も作ってしまいます。

最初は `ShaderCache` から導入し、
そのパターンを `RootSignatureCache` と `PipelineLibrary` に広げるのが安全です。
