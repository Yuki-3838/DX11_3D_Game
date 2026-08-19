"""Analyze a gameplay video with the Gemini API.

Usage:
    python tools/analyze_video.py path/to/bug.mp4

The API key is read only from GEMINI_API_KEY.
"""

from __future__ import annotations

import argparse
import os
import shutil
import sys
import tempfile
import time
from pathlib import Path


DEFAULT_MODEL = "gemini-3.6-flash"
PROCESSING_POLL_SECONDS = 5

ANALYSIS_PROMPT = """
You are analyzing a gameplay bug report video for a C++ DirectX 11 game.
Analyze the video chronologically from beginning to end. Do not infer hidden
code behavior from appearance alone. Clearly separate observations that are
directly visible in the video from hypotheses that need source-code validation.

Return a Japanese Markdown report with exactly these sections:

# 動画解析レポート
## 1. 動画全体で起きていること
## 2. 時系列
Use timestamps in [分:秒] format. Describe the important visible events.
## 3. 観察した事実
Only facts directly confirmed by visible frames or audio. If something cannot
be confirmed, say so explicitly.
## 4. 異常な挙動
Describe what differs from expected gameplay behavior.
## 5. 異常が始まった時刻
Give the earliest timestamp where the abnormal behavior is visible, with an
uncertainty range if exact timing is not possible.
## 6. 異常発生直前と直後の変化
Compare the state immediately before and after the problem begins.
## 7. 原因として考えられる処理（推測）
This section must contain hypotheses only. Label each hypothesis with a
confidence level and explain what evidence supports it.
## 8. 調査すべきコード（推測）
Suggest likely categories, functions, or data flows to inspect. Do not claim
that a specific source line is the cause because source code is not provided
to you.
## 9. 追加で確認したいこと
List any missing camera angle, debug overlay, input, timing, or reproduction
information that would disambiguate the hypotheses.

Pay special attention to enemy AI state changes, attack windups, attack hit
windows, collision/overlap behavior, animation playback, movement speed, and
whether an apparent hit is actually a collision push or a damage event.
""".strip()


def configure_console_encoding() -> None:
    """Keep Japanese/Unicode reports printable on a CP932 Windows console."""
    for stream in (sys.stdout, sys.stderr):
        reconfigure = getattr(stream, "reconfigure", None)
        if reconfigure is not None:
            reconfigure(encoding="utf-8", errors="replace")


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Analyze an MP4 gameplay bug video with Gemini.")
    parser.add_argument("video", type=Path, help="Path to an MP4 video")
    parser.add_argument(
        "-o",
        "--output",
        type=Path,
        help="Markdown output path (default: <video>.analysis.md)",
    )
    parser.add_argument(
        "--model",
        default=os.environ.get("GEMINI_MODEL", DEFAULT_MODEL),
        help=f"Gemini model (default: GEMINI_MODEL or {DEFAULT_MODEL})",
    )
    return parser.parse_args()


def wait_until_active(client, uploaded_file):
    """Wait until the Files API has finished processing the video."""
    while True:
        state = getattr(getattr(uploaded_file, "state", None), "name", None)
        if state == "ACTIVE":
            return uploaded_file
        if state in {"FAILED", "ERROR"}:
            raise RuntimeError(f"Gemini video processing failed: {state}")

        print(f"Gemini video processing: {state or 'PENDING'}...", file=sys.stderr)
        time.sleep(PROCESSING_POLL_SECONDS)
        uploaded_file = client.files.get(name=uploaded_file.name)


def output_path_for(video_path: Path, requested: Path | None) -> Path:
    if requested is not None:
        return requested
    return video_path.with_suffix(video_path.suffix + ".analysis.md")


def main() -> int:
    configure_console_encoding()
    args = parse_args()
    video_path = args.video.expanduser().resolve()

    if not video_path.is_file():
        print(f"動画ファイルが見つかりません: {video_path}", file=sys.stderr)
        return 2
    if video_path.suffix.lower() != ".mp4":
        print("MP4ファイルを指定してください。", file=sys.stderr)
        return 2

    api_key = os.environ.get("GEMINI_API_KEY")
    if not api_key:
        print("環境変数 GEMINI_API_KEY が設定されていません。", file=sys.stderr)
        return 2

    try:
        from google import genai
    except ImportError:
        print(
            "google-genai がインストールされていません。"
            " `python -m pip install -r tools/requirements.txt` を実行してください。",
            file=sys.stderr,
        )
        return 3

    temporary_upload_dir = None
    upload_path = video_path
    try:
        # Work around SDK/runtime combinations that cannot encode Unicode local
        # paths while preserving the user's original path for the report.
        str(video_path).encode("ascii")
    except UnicodeEncodeError:
        temporary_upload_dir = tempfile.TemporaryDirectory(prefix="gemini_video_")
        upload_path = Path(temporary_upload_dir.name) / "gameplay_bug.mp4"
        shutil.copy2(video_path, upload_path)

    try:
        client = genai.Client(api_key=api_key)
        print(f"Geminiへ動画をアップロード中: {video_path}", file=sys.stderr)
        uploaded_file = client.files.upload(
            file=str(upload_path),
            # Some Windows/SDK combinations encode display_name as ASCII.
            # Keep the Unicode local path, but use a safe API-side display name.
            config={"mime_type": "video/mp4", "display_name": "gameplay_bug.mp4"},
        )
        uploaded_file = wait_until_active(client, uploaded_file)
        print(f"Geminiで解析中: model={args.model}", file=sys.stderr)
        response = client.interactions.create(
            model=args.model,
            input=[
                {
                    "type": "video",
                    "uri": uploaded_file.uri,
                    "mime_type": uploaded_file.mime_type or "video/mp4",
                },
                {"type": "text", "text": ANALYSIS_PROMPT},
            ],
        )
    except Exception as exc:  # SDK errors vary between API versions.
        print(f"Gemini API呼び出しに失敗しました: {exc}", file=sys.stderr)
        if temporary_upload_dir is not None:
            temporary_upload_dir.cleanup()
        return 1

    if temporary_upload_dir is not None:
        temporary_upload_dir.cleanup()

    report = response.output_text or "（Geminiから解析結果が返りませんでした）"
    destination = output_path_for(video_path, args.output)
    destination.parent.mkdir(parents=True, exist_ok=True)
    destination.write_text(
        "<!-- Generated by tools/analyze_video.py; verify against source code. -->\n"
        f"<!-- Video: {video_path} -->\n"
        f"<!-- Model: {args.model} -->\n\n"
        + report
        + "\n",
        encoding="utf-8",
    )
    print(report)
    print(f"\n解析結果を保存しました: {destination}", file=sys.stderr)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
