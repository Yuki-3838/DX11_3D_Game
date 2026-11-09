#pragma once

#include <string>
#include <utility>

namespace GameFlow
{
    enum class Result
    {
        Victory,
        Defeat
    };

    inline Result lastResult = Result::Defeat;
    inline std::string requestedScene{};

    /**
     * @brief タイトルとリザルトを使うか(ゲームループ)。
     *
     * true  : タイトル → ゲーム → リザルト → タイトル(提出・お披露目の通常の形)
     * false : ゲーム本編だけ。決着がついたら同じ戦いをやり直す。
     *         動きや手触りを続けて見たいときに、毎回タイトルへ戻らずに済む。
     *
     * 起動時に dev_settings.ini の gameloop で決める。デバッグ表示からも切り替えられる。
     */
    inline bool useGameLoop = true;
    // 本編だけのときに、決着後へ戦いをやり直すための合図。
    inline bool restartRequested = false;
    /**
     * @brief 起動したら、ゲーム本編で攻撃のタイムラインエディター(F6)を開いた状態にする。
     * 起動引数 --timeline で立てる(VSの構成プルダウンの「攻撃タイムライン」)。
     * 最初の1回だけ使う(リトライのたびに開き直さないよう、使ったら下ろす)。
     */
    inline bool openAttackTimelineOnStart = false;

    /**
     * @brief 調整・撮影用。タイトルとリザルトを、ボタンを押さずに自動で進める。
     * 起動時に dev_settings.ini の auto_start=1 で立てる。
     * 撮影のために外からキー入力を送らずに、タイトルからの一連の流れを通すため。
     */
    inline bool autoStart = false;

    /**
     * @brief 次にゲーム本編へ入るとき、飛来の場面(ローディング画面)を流す合図。
     * タイトルとリザルトから入るときに立てる。使ったら下ろす。
     * 本編だけで動かしているとき(gameloop=0・F1)は立てないので、調整のたびに待たされない。
     */
    inline bool flightIntroRequested = false;

    inline void RequestFlightIntro()
    {
        flightIntroRequested = true;
    }

    inline bool ConsumeFlightIntroRequest()
    {
        const bool result = flightIntroRequested;
        flightIntroRequested = false;
        return result;
    }

    inline void SetResult(Result result)
    {
        lastResult = result;
    }

    inline Result GetResult()
    {
        return lastResult;
    }

    inline void RequestScene(std::string sceneName)
    {
        requestedScene = std::move(sceneName);
    }

    inline std::string ConsumeRequestedScene()
    {
        std::string result = std::move(requestedScene);
        requestedScene.clear();
        return result;
    }

    inline void RequestRestart()
    {
        restartRequested = true;
    }

    inline bool ConsumeRestart()
    {
        const bool result = restartRequested;
        restartRequested = false;
        return result;
    }
}
