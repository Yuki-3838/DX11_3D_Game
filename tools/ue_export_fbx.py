"""Unreal Editor 内で実行し、購入アセットのモデルとアニメーションを FBX へ一括で書き出す。

Fab のアセットにはアニメーションが Unreal Engine 形式(.uasset)にしか入っていないものがある
(例: Wyvern spiked dragon lizard)。このゲームは Assimp で FBX を読むので、
Unreal Editor で開いて FBX に書き出してから取り込む。

使い方(Unreal Editor 5.x):
  1. Edit > Plugins で「Python Editor Script Plugin」と「Editor Scripting Utilities」を有効にして再起動する。
  2. Tools > Execute Python Script... でこのファイルを選ぶ。
     または Output Log の入力欄を「Python」に切り替え、次を実行する:
       py "C:/path/to/ue_export_fbx.py"
  3. 下の OUTPUT_DIR に FBX と一覧(export_list.txt)ができる。

エディタを開かずにコマンドラインで実行する場合(-NullRHI は不可。メッシュの書き出しで落ちる):
  UnrealEditor-Cmd.exe "<Project>.uproject" -run=pythonscript -script="<このファイル>"
      -unattended -nop4 -nosplash -RenderOffscreen -stdout

書き出すもの:
  - SkeletalMesh: メッシュと骨格(モデル本体)。
  - AnimSequence: 1クリップ1ファイル。メッシュは含めず骨の動きだけを書き出す
    (このゲームはモデルとモーションを別ファイルで読み込む)。

注意: 書き出した FBX は購入した有料アセットなので、公開リポジトリへコミットしないこと。
"""

import os

import unreal

# 探す場所(Content Browser のパス)。Fab から追加したアセットは /Game の下に入る。
SOURCE_ROOT = "/Game"
# アセットのパスにこの文字列を含むものだけを書き出す(大文字小文字は区別しない)。空なら全部。
NAME_FILTER = "wyvern"
# 書き出し先。既定はダウンロードフォルダの下。
OUTPUT_DIR = os.path.join(os.environ.get("USERPROFILE", os.path.expanduser("~")), "Downloads", "UE_FBX_Export")


def _set_option(options, name, value):
    """エンジンのバージョンによって無いプロパティがあるため、無ければ飛ばす。"""
    try:
        options.set_editor_property(name, value)
    except Exception:  # noqa: BLE001 - Unreal はプロパティ不在で汎用例外を投げる
        unreal.log_warning("FbxExportOption に {} がないため設定を省略".format(name))


def _make_options(include_mesh):
    options = unreal.FbxExportOption()
    _set_option(options, "ascii", False)
    _set_option(options, "collision", False)
    _set_option(options, "level_of_detail", False)
    _set_option(options, "export_morph_targets", True)
    # アニメーションの FBX にプレビュー用メッシュを含めるか。
    _set_option(options, "export_preview_mesh", include_mesh)
    # ルートの移動を骨の動きとして残す(In-place のクリップなら変化しない)。
    _set_option(options, "map_skeletal_motion_to_root", False)
    return options


def _export(asset, exporter, filename, include_mesh):
    task = unreal.AssetExportTask()
    task.set_editor_property("object", asset)
    task.set_editor_property("exporter", exporter)
    task.set_editor_property("filename", filename)
    task.set_editor_property("automated", True)
    task.set_editor_property("prompt", False)
    task.set_editor_property("replace_identical", True)
    task.set_editor_property("options", _make_options(include_mesh))
    return unreal.Exporter.run_asset_export_task(task)


def _clip_length(anim):
    for getter in ("get_play_length",):
        if hasattr(anim, getter):
            try:
                return float(getattr(anim, getter)())
            except Exception:  # noqa: BLE001
                pass
    try:
        return float(anim.get_editor_property("sequence_length"))
    except Exception:  # noqa: BLE001
        return -1.0


def main():
    paths = unreal.EditorAssetLibrary.list_assets(SOURCE_ROOT, recursive=True, include_folder=False)
    if NAME_FILTER:
        paths = [p for p in paths if NAME_FILTER.lower() in p.lower()]
    if not paths:
        unreal.log_error("{} の下に '{}' を含むアセットが見つからない。NAME_FILTER を確認すること。".format(SOURCE_ROOT, NAME_FILTER))
        return

    meshes, anims = [], []
    for path in paths:
        asset = unreal.EditorAssetLibrary.load_asset(path)
        if isinstance(asset, unreal.SkeletalMesh):
            meshes.append(asset)
        elif isinstance(asset, unreal.AnimSequence):
            anims.append(asset)

    os.makedirs(os.path.join(OUTPUT_DIR, "anim"), exist_ok=True)
    lines = ["# {} から書き出し".format(SOURCE_ROOT), ""]
    failed = 0

    list_path = os.path.join(OUTPUT_DIR, "export_list.txt")

    def record(line):
        # 書き出し中にエンジンが落ちても、そこまでの結果が残るように1件ずつ書く。
        lines.append(line)
        with open(list_path, "w", encoding="utf-8") as f:
            f.write("\n".join(lines) + "\n")

    # アニメーションを先に書き出す。メッシュの書き出しは描画機能を使うため、
    # -NullRHI で起動すると SkinnedMeshComponent のアサートで落ちる(-RenderOffscreen で起動すること)。
    for anim in sorted(anims, key=lambda a: a.get_name()):
        filename = os.path.join(OUTPUT_DIR, "anim", anim.get_name() + ".fbx")
        ok = _export(anim, unreal.AnimSequenceExporterFBX(), filename, False)
        failed += 0 if ok else 1
        record("{}\tanim\t{:.3f}s\t{}".format("OK" if ok else "FAILED", _clip_length(anim), filename))

    for mesh in meshes:
        filename = os.path.join(OUTPUT_DIR, mesh.get_name() + ".fbx")
        ok = _export(mesh, unreal.SkeletalMeshExporterFBX(), filename, True)
        failed += 0 if ok else 1
        record("{}\tmesh\t{}".format("OK" if ok else "FAILED", filename))

    unreal.log("FBX 書き出し完了: メッシュ {} 件 / アニメーション {} 件 / 失敗 {} 件 -> {}".format(
        len(meshes), len(anims), failed, OUTPUT_DIR))


main()
