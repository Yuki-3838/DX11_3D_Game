# Fallen Paladin プレイヤー候補

## 出典

- 作品名: Fallen Paladin in Corrupted Black Plate Armor
- 作者: Pigcraft (`@s8819296`)
- Sketchfab: https://sketchfab.com/3d-models/fallen-paladin-in-corrupted-black-plate-armor-df3056caff7e4f079bd4eec0fa9bcd54
- ライセンス: Creative Commons Attribution 4.0 International (CC BY 4.0)
- ライセンス本文: https://creativecommons.org/licenses/by/4.0/

## 利用条件

Sketchfab の配布表示では「作者のクレジット表記が必要。商用利用可」となっている。
ゲームへ収録・改変する場合は、タイトル画面またはクレジット画面に作者名、作品名、出典URL、ライセンスURLを掲載する。
改変版を使う場合は、軽量化・リギング・ウェイト調整などの変更を行ったことも明記する。

## 導入方針

配布元の原本はこのディレクトリ内で `source/` に保管し、ゲームが直接読む加工済みデータは `runtime/` に分ける。
原本を上書きしないことで、軽量化・リギング・マテリアル調整をやり直せるようにする。

配布モデルは高ポリゴンかつ未リギングだったため、原本を上書きせず、ゲームのモーションFBXから取り出したMixamo骨格に合わせてウェイト付けしたランタイム版を生成した。
ゲーム用GLBは約20万頂点へ軽量化し、全頂点に最大4本以内のボーン影響を設定している。原本の質感テクスチャと剣も同じメッシュ内に保持している。
ウェイトはゲームの待機姿勢（上半身だけをアニメーション、腰・脚・頭はレスト姿勢）での骨位置から求める。肘は関節の中心点だけでなく、装甲の断面を覆う短い帯状範囲で上腕・前腕を混合し、腕を曲げたときの装甲の分離を抑える。元モデルの下げた腕とモーション骨格の差は、このゲーム内待機姿勢へ逆バインドして補正する。
プレイヤーのインポートモーションは、Mixamoのキー回転を各ボーンのレスト回転へ合成してから階層計算する。これにより前腕がレスト姿勢のローカル軸を失わず、肘が上腕に対して連動する。
ゲーム内の向き補正は描画・剣の変換だけへ適用し、プレイヤーの移動方向や回避方向は変えない。

## 現在の配置

- 原本FBX: `source/tripo_convert_3786d797-fc30-4fb0-9746-8a6f87e0ac71.fbx`
- テクスチャ: `source/tripo_convert_3786d797-fc30-4fb0-9746-8a6f87e0ac71.fbm/`
- 実行用GLB: `runtime/fallen_paladin_rigged.glb`
- 編集用Blenderファイル: `runtime/fallen_paladin_rigged.blend`
- 軽量化確認用Blenderファイル: `runtime/fallen_paladin_runtime_optimized.blend`
- 現在のゲーム内適用先: プレイヤーのリグ付きモデル
- ボーン名: 既存モーションと同じ66本のMixamo骨格。左右の手指・剣ボーンも含む
- ウェイト: 実際の待機姿勢での最寄り骨を基本に、関節の親子骨だけを近傍で補間
- 既存の歩行・走行・待機・剣攻撃モーションをゲーム側から読み込む。
- 原本と同じメッシュ内の剣を使用し、外部の簡易剣モデルは追加しない。

原本のZIP、展開済みFBX、加工スクリプトは `source/` と `tools/` に残し、再加工できるようにしている。

## ゲーム内クレジット文

```text
Fallen Paladin in Corrupted Black Plate Armor
by Pigcraft (@s8819296)
Licensed under CC BY 4.0
https://sketchfab.com/3d-models/fallen-paladin-in-corrupted-black-plate-armor-df3056caff7e4f079bd4eec0fa9bcd54
https://creativecommons.org/licenses/by/4.0/

Modified for this game: optimized, rigged, weighted, and adapted for gameplay.
```
