# プロジェクト同梱の外部依存

このフォルダには、Visual Studioでプロジェクトをビルドするために必要な外部ライブラリを配置します。
プロジェクトファイルはこのフォルダを基準にした相対パスを参照するため、別のPCへ移動しても
ドライブ名やユーザー名に依存しません。

## リポジトリに入っているもの

- `assimp/include`: Assimpのヘッダ
- `assimp/lib/assimp-vc143-mtd.lib`: Debug構成でリンクするAssimp(デバッグ版)
- `assimp/lib/assimp-vc142-mt.lib`: Release構成でリンクするAssimp(リリース版)
- `assimp/lib/zlibstaticd.lib`: Assimpが依存するzlib
- `DirectXTK/include`: DirectX Tool Kitのヘッダ

実行時に必要なAssimpのDLLは、**リポジトリのルート**に置いてあります
(`assimp-vc143-mtd.dll` / `assimp-vc142-mt.dll`)。
ゲームは作業フォルダをリポジトリのルートにして起動するため、この位置で解決されます。

## リポジトリに入れていないもの(入れなくてもビルドできる)

- `DirectXTK/lib/**`(合計約110MB)
  このプロジェクトはDirectXTKのうち`SimpleMath`のヘッダしか使っていません。
  ヘッダだけでは解決できない`Matrix::Identity`などの定数は
  [system/SimpleMathIdentity.cpp](../system/SimpleMathIdentity.cpp)で自前定義しているため、
  **DirectXTK.libはリンクしていません**。容量を理由に追跡対象から外しています。
  (`.vcxproj`のライブラリ検索パスには残っていますが、参照されるlibはありません。)
- `assimp/bin/**`
  同じDLLがリポジトリのルートにあるため不要です。

## 検証済みの手順

GitHubからクローンした直後の状態で、`Debug|x64`と`Release|x64`のどちらも
追加の手作業なしにビルドが通り、起動してモデルが読み込まれることを確認しています。

## Assimpを構成で使い分けている理由

`assimp-vc143-mtd.dll`(デバッグ版)は`ucrtbased.dll` / `MSVCP140D.dll`という
**デバッグ版CRT**へ依存します。これはVisual Studioが入っているPCにしか存在せず、
再頒布も許可されていません。そのため、これを配布物に入れると
Visual Studioの無いPCでは起動できません。

Release構成ではリリース版の`assimp-vc142-mt.dll`をリンクしており、
こちらは再頒布可能な`MSVCP140.dll`だけで動きます。
配布用のフォルダは[MakeDistribution.bat](../MakeDistribution.bat)が作ります。

## ビルドに必要な環境

Visual Studio 2022、MSVC v143、Windows 10 SDK。
モーションエディタを含むソリューション全体をビルドする場合は、
.NET 8 SDKとWPFの開発環境も必要です。
