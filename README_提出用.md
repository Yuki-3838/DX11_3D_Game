# 提出用プロジェクト

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

## 起動時の注意

ゲームは`assets`と`shader`を実行時の作業フォルダから読み込む。実行ファイルだけを別フォルダへ移動せず、プロジェクトのルートフォルダを作業フォルダにして起動すること。

実行環境として提出する場合は、実行ファイル、`assets`、`shader`、AssimpのDLLを同じ提出フォルダに配置する。

現在同梱しているAssimpは`assimp-vc143-mtd.dll`（デバッグランタイム版）である。別のPCで確実に実行する最終版では、Release版Assimpに差し替え、対応するDLLを実行ファイルと同じフォルダに配置する。

## ZIPから除外するもの

- `.vs`
- `x64`、`x86`
- `Debug`、`Release`
- `bin`、`obj`
- `*.obj`、`*.pdb`、`*.iobj`、`*.ipdb`
- `.tlog`
