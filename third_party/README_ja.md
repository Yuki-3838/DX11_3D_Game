# プロジェクト同梱の外部依存

このフォルダには、Visual Studioでプロジェクトをビルドするために必要な外部ライブラリを配置します。

- `DirectXTK/include`: DirectX Tool Kitのヘッダ
- `DirectXTK/lib/Win32`、`DirectXTK/lib/x64`: 構成別のDirectX Tool Kitライブラリ
- `assimp/include`: Assimpのヘッダ
- `assimp/lib`: Assimpと依存するzlibのライブラリ
- `assimp/bin`: 実行時に必要なAssimp DLL

プロジェクトファイルは、このフォルダを基準にした相対パスを参照します。別のPCへ移動しても、ドライブ名やユーザー名に依存しない構成です。

ビルドにはVisual Studio 2022、MSVC v143、Windows 10 SDKが必要です。モーションエディタを含むソリューション全体をビルドする場合は、.NET 8 SDKとWPFの開発環境も必要です。
