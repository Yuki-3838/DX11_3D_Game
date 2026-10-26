# 提出用プロジェクト

## 操作方法

キーボード・マウスとゲームパッド(Xbox配列のXInput対応パッド)のどちらでも遊べる。
ゲームパッドがつながっている間は、戦闘中の画面右下とタイトル・リザルトの操作説明がパッドの表記になる(キーボード・マウスでも操作はできる)。

| 操作 | キーボード・マウス | ゲームパッド |
|---|---|---|
| 移動 | WASD | 左スティック |
| 視点 | マウス | 右スティック |
| 弱攻撃(連打でコンボ) | 左クリック | RB |
| 強攻撃 | 右クリック | RT |
| 回避 | Space | A |
| ダッシュ | Shift(押し続ける) | LB(押し続ける) |
| ロックオン | Tab | 右スティック押し込み |
| 決定(タイトル・リザルト) | Enter / Space | A / Start |
| タイトルへ戻る(リザルト) | Esc | B |

ダッシュ中に弱攻撃でダッシュ攻撃になる。左Altでマウスカーソルの表示を切り替える(デバッグ表示を触るとき)。

## ビルド環境

- Windows 10/11
- Visual Studio 2022
- 「C++によるデスクトップ開発」
- MSVC v143
- Windows 10 SDK
- モーションエディタもビルドする場合は .NET 8 SDK と WPF

## ゲーム本体のビルド

1. `DX11_3D_Game.sln`をVisual Studio 2022で開く。
2. 構成を`Release`、プラットフォームを`x64`にする。
3. ビルドする。

外部ライブラリは`third_party`に同梱している。プロジェクト設定はドライブ名に依存しない相対パスを参照する。
クローンした直後の状態で`Debug|x64`・`Release|x64`ともにビルドが通ることを確認済み
(詳細は[third_party/README_ja.md](third_party/README_ja.md))。

## 起動時の注意

ゲームは`assets`と`shader`を実行時の作業フォルダから読み込む。実行ファイルだけを別フォルダへ移動せず、プロジェクトのルートフォルダを作業フォルダにして起動すること。
Visual Studioから実行する場合は、プロジェクト設定でルートフォルダが作業フォルダになっている。

## 別のPCで動かす

### Visual Studioが入っているPC

クローンしてソリューションを開き、`Release|x64`でビルドして実行するだけでよい。
追加のセットアップは不要。

### Visual Studioが入っていないPC

`MakeDistribution.bat`を「Developer Command Prompt for VS 2022」から実行すると、
`dist`フォルダに実行ファイル・Assimpのリリース版DLL・`shader`・`assets`をまとめる。
`dist`フォルダごと渡し、中の`DX11_3D_Game.exe`を実行してもらう。

渡す先のPCに必要なもの:

- Windows 10/11（64bit）
- [Microsoft Visual C++ 再頒布可能パッケージ (x64)](https://aka.ms/vs/17/release/vc_redist.x64.exe)
- DirectX 11が動作するGPU

**Debug構成のビルドを配ってはいけない。** Debug構成はデバッグ版CRT
（`ucrtbased.dll` / `MSVCP140D.dll`）を必要とし、これはVisual Studioが入っているPCにしか
存在せず再頒布も許可されていない。Release構成はリリース版Assimpをリンクしており、
再頒布可能CRTだけで動く。

## ZIPから除外するもの

- `.vs`
- `x64`、`x86`
- `Debug`、`Release`
- `bin`、`obj`
- `*.obj`、`*.pdb`、`*.iobj`、`*.ipdb`
- `.tlog`
