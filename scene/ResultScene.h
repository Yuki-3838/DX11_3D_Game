#pragma once

#include "../system/IScene.h"
#include "../system/SceneClassFactory.h"

class ResultScene final : public IScene
{
public:
    void update(uint64_t deltatime) override;
    void draw(uint64_t deltatime) override;
    void init() override;
    void dispose() override;

private:
    // リトライが決まった。次のフレームで読み込みに入る(その前に真っ暗な画面を1回描く)。
    bool m_retrying = false;
    // 調整・撮影用の経過秒(dev_settings.ini の auto_start)。
    float m_elapsedSeconds = 0.0f;
};

REGISTER_CLASS(ResultScene)
