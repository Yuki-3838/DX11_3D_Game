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

};

REGISTER_CLASS(ResultScene)
