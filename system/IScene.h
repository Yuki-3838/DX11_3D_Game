#pragma once
#include <cstdint>

// シーンインタフェース
class IScene {
public:
	IScene() = default;
	virtual ~IScene() = default;
	virtual void update(uint64_t delta) = 0;
	virtual void draw(uint64_t delta) = 0;
	virtual void init() = 0;
	virtual void dispose() = 0;
};
