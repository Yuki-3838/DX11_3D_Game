#pragma once

#include "CombatDesign.h"
#include "PerformanceProfiler.h"

#include <string>

/**
 * @class BrowserDebugReporter
 * @brief ブラウザで確認できるデバッグ情報を書き出す。
 *
 * ゲーム起動中に `debug/debug_state.json` を更新し、`debug/index.html` から
 * fetchで読みます。ローカルサーバーがなくても、将来のHTTPデバッグ表示へ移行しやすい
 * ファイル構成にしています。
 */
class BrowserDebugReporter
{
public:
    /** @brief 出力先ディレクトリを設定し、HTMLを生成する。 */
    void Init(const std::wstring& outputDirectory);

    /** @brief 最新のデバッグスナップショットをJSONへ書き出す。 */
    void Write(const PerformanceSnapshot& snapshot, const Combat::CombatDebugState& combatState);

private:
    void WriteHtml() const;

private:
    std::wstring m_outputDirectory = L"debug";
};
