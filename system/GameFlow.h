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
}
