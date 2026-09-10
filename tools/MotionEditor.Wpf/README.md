# DX11 Motion Editor (WPF)

`DX11_MOTION 1` 形式の `.motion` ファイルを編集する外部ツールです。

プロジェクトルートから次のコマンドで起動できます。

```powershell
dotnet run --project tools/MotionEditor.Wpf/MotionEditor.Wpf.csproj
```

または、プロジェクトルートの `StartMotionEditor.bat` をダブルクリックしてください。起動場所に関係なくプロジェクトルートを設定し、未ビルドなら自動でビルドします。

主な操作：

- `.motion` ファイルの一覧、読み込み、保存、名前を付けて保存
- ボーン選択、タイムラインのスクラブ、キーのドラッグ移動
- 右クリックでキー削除、追加・複製
- 回転・移動・拡縮の数値入力を即時反映
- ポーズプレビューのマウスギズモ操作
- Undo / Redo
- 自動保存と `assets/motion/selected_attack.txt` の更新

ゲーム本体の3Dプレビューは従来どおりDX11側で行います。WPFエディタで保存したモーションは、ゲームを再起動すると選択攻撃として読み込まれます。
