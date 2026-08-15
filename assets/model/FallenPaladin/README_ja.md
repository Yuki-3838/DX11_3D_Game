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

配布モデルは高ポリゴンかつ未リギングのため、そのままプレイヤーへ組み込まない。
まず軽量化、ボーン作成、ウェイト付け、アニメーション用のボーン名整理、テクスチャ解像度調整を行い、既存プレイヤーの攻撃モーションと当たり判定が利用できる形にする。

## 現在の加工版

- 実行時モデル: `runtime/FallenPaladin_Player_clean.glb`
- 加工内容: 約199万三角形から約15.9万三角形へ削減、身長を既存プレイヤー基準の約2.9へ調整、19本の人型ボーンを作成、ウェイト付け、正面軸をゲーム座標へ合わせて90度補正、足元をY=0へ配置
- ボーン名: `upperarm.l/r`, `lowerarm.l/r`, `hand.l/r`, `upperleg.l/r`, `lowerleg.l/r`, `foot.l/r` など、既存の攻撃モーションが解決できる名前を使用
- 加工ツール: Blender 4.5.12 LTS ポータブル版

原本のZIP、展開済みFBX、加工スクリプトは `source/` と `tools/blender/` に残し、再加工できるようにしている。

## ゲーム内クレジット文

```text
Fallen Paladin in Corrupted Black Plate Armor
by Pigcraft (@s8819296)
Licensed under CC BY 4.0
https://sketchfab.com/3d-models/fallen-paladin-in-corrupted-black-plate-armor-df3056caff7e4f079bd4eec0fa9bcd54
https://creativecommons.org/licenses/by/4.0/

Modified for this game: optimized, rigged, weighted, and adapted for gameplay.
```
