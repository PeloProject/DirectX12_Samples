# DirectX12_Samples 詳細設計資料 - 目次

作成日: 2026-06-14

## ドキュメント一覧

| # | ファイル | 内容 |
|---|--------|------|
| 01 | [プロジェクト概要](01_プロジェクト概要.md) | プロジェクト全体の目的・技術スタック・コンポーネント図 |
| 02 | [レイヤーアーキテクチャ](02_レイヤーアーキテクチャ.md) | 3層構成とモジュール間依存関係図 |
| 03 | [クラス図_ApplicationDLL](03_クラス図_ApplicationDLL.md) | C++ネイティブ側の全クラス設計 |
| 04 | [クラス図_PieGameManaged](04_クラス図_PieGameManaged.md) | C#マネージド側の全クラス設計 |
| 05 | [シーケンス図_初期化](05_シーケンス図_初期化.md) | 起動〜描画開始までの初期化フロー |
| 06 | [シーケンス図_フレームループ](06_シーケンス図_フレームループ.md) | 毎フレームの処理順序 |
| 07 | [シーケンス図_PIE](07_シーケンス図_PIE.md) | Play In Editor の開始・停止・ホットリロード |
| 08 | [API仕様](08_API仕様.md) | ApplicationDLL 公開API と PieGameManaged エクスポート一覧 |

## システム概要

```
┌──────────────────────────────────────────┐
│  ホスト層                                 │
│  EditorQt (Qt6 UI)  /  ApplicationDLLHost │
├──────────────────────────────────────────┤
│  ネイティブランタイム層                    │
│  ApplicationDLL.dll (DirectX12/Vulkan/GL) │
├──────────────────────────────────────────┤
│  マネージドゲーム層                        │
│  PieGameManaged.dll (C# NativeAOT)        │
└──────────────────────────────────────────┘
```

## PlantUML のレンダリングについて

各ドキュメントの図は PlantUML 形式で記述されています。  
VSCode では [PlantUML 拡張](https://marketplace.visualstudio.com/items?itemName=jebbs.plantuml) を使用することでプレビューできます。  
オンラインでは https://www.plantuml.com/plantuml/ でレンダリング可能です。
