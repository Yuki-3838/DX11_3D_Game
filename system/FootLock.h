#pragma once

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

#include "commontypes.h"

/**
 * @file FootLock.h
 * @brief 接地している足のつま先をワールドに固定する(フットロック)。
 *
 * 参考: Daniel Holden「Inverse Kinematics and Foot Locking」
 * https://theorangeduck.com/page/inverse-kinematics-foot-locking
 *
 * 足が滑るのは「モーションの足の速さ」と「キャラクターが実際に進む速さ」が食い違うから。
 * そこで、接地している間はつま先の位置をワールドに固定し、その位置へ脚を2ボーンIKで合わせる。
 *
 * 記事から取り入れた考え方:
 *  - 接地の判定は**高さではなく速さ**で行う。移動と足の運びが合っていれば、接地中のつま先は
 *    ワールドではほとんど止まって見える。地面の高さだけで判定すると、滑っていても接地と見なしてしまう。
 *  - 固定するのは**つま先**。足首(かかと)を固定すると、腰が下がって膝が過剰に曲がった姿勢になりやすい。
 *  - ロックを外すときは位置を急に戻さず、短い時間で補間する。
 *  - 「少し滑るほうが、元のモーションを壊すよりずっとまし」。目標へ届かないときは無理に伸ばさない
 *    (脚の長さの制限はIK側で滑らかに掛けている)。
 *
 * 単位はこのゲームのワールド単位(プレイヤーの身長が約18、1メートルが約10)。
 */
namespace FootLock
{

struct Settings
{
	// このワールド速度(単位/秒)を下回り、かつ地面の近くにあるとロックする。
	// 歩き20・ダッシュ45に対し、接地中のつま先はほぼ止まる。踏み出し中は40以上出る。
	float lockSpeed = 8.0f;
	// ロック中に、モーション側のつま先がこの速さを超えたら離れたと見なす。
	float unlockSpeed = 16.0f;
	// ロック位置からこれだけ離れたら、無理に引っ張らずに解除する。
	// 脚の長さ(股からつま先まで約11)を超える目標は届かず、届かないまま固定し続けると
	// 脚が伸びきったまま結局滑る。届く範囲で解除する。
	float unlockDistance = 9.0f;
	// 接地と見なす高さ(足元からの高さ)。
	float groundContactHeight = 4.0f;
	// 解除したときに、ずれを戻すのにかける時間(秒)。
	float releaseBlendSeconds = 0.15f;
	// 地面の高さ(ワールドY)。
	float groundY = -0.3f;
};

/** 片足分の状態。 */
struct FootState
{
	bool locked = false;
	Vector3 lockedPosition{ 0.0f, 0.0f, 0.0f };
	// 解除したときのずれ。releaseBlendSecondsかけて0へ戻す。
	Vector3 releaseOffset{ 0.0f, 0.0f, 0.0f };
	float releaseTime = 0.0f;
	Vector3 previousAnimatedPosition{ 0.0f, 0.0f, 0.0f };
	bool hasPreviousPosition = false;
	// 直近の速さ(デバッグ表示用)。
	float speed = 0.0f;

	void Reset()
	{
		locked = false;
		releaseOffset = Vector3(0.0f, 0.0f, 0.0f);
		releaseTime = 0.0f;
		hasPreviousPosition = false;
		speed = 0.0f;
	}
};

/**
 * @brief 1フレーム分の更新。モーションどおりのつま先の位置から、IKの目標位置を返す。
 * @param state 片足の状態。
 * @param animatedWorldToe モーションどおりのつま先のワールド位置。
 * @param deltaSeconds 経過時間。
 * @param settings 調整値。
 * @return IKで合わせにいくワールド位置。
 */
inline Vector3 Update(
	FootState& state,
	const Vector3& animatedWorldToe,
	float deltaSeconds,
	const Settings& settings)
{
	if (!state.hasPreviousPosition || deltaSeconds <= 0.0001f)
	{
		state.previousAnimatedPosition = animatedWorldToe;
		state.hasPreviousPosition = true;
		state.speed = 0.0f;
		return animatedWorldToe;
	}

	state.speed = (animatedWorldToe - state.previousAnimatedPosition).Length() / deltaSeconds;
	state.previousAnimatedPosition = animatedWorldToe;

	if (state.locked)
	{
		const float distance = (animatedWorldToe - state.lockedPosition).Length();
		if (state.speed > settings.unlockSpeed || distance > settings.unlockDistance)
		{
			// 解除。今のずれを覚えておき、少しずつ0へ戻す(急に戻すと足が跳ねる)。
			state.locked = false;
			state.releaseOffset = state.lockedPosition - animatedWorldToe;
			state.releaseTime = 0.0f;
		}
		else
		{
			return state.lockedPosition;
		}
	}
	else if (state.speed < settings.lockSpeed &&
		animatedWorldToe.y <= settings.groundY + settings.groundContactHeight)
	{
		state.locked = true;
		state.lockedPosition = Vector3(
			animatedWorldToe.x,
			std::max(animatedWorldToe.y, settings.groundY),
			animatedWorldToe.z);
		state.releaseOffset = Vector3(0.0f, 0.0f, 0.0f);
		return state.lockedPosition;
	}

	// 解除直後のずれを、なめらかに0へ戻す。
	if (state.releaseOffset.LengthSquared() > 0.0001f)
	{
		state.releaseTime += deltaSeconds;
		const float linear = std::clamp(
			state.releaseTime / std::max(settings.releaseBlendSeconds, 0.001f), 0.0f, 1.0f);
		// smoothstepで戻す(速度が急に変わらないように)。
		const float weight = 1.0f - linear * linear * (3.0f - 2.0f * linear);
		if (linear >= 1.0f)
			state.releaseOffset = Vector3(0.0f, 0.0f, 0.0f);
		return animatedWorldToe + state.releaseOffset * weight;
	}
	return animatedWorldToe;
}

/** 足1本分の骨の名前(Mixamoの命名を想定しつつ、部分一致で探す)。 */
struct LegBoneNames
{
	std::string upperLeg;
	std::string lowerLeg;
	std::string foot;
	std::string toe;

	bool IsValid() const
	{
		return !upperLeg.empty() && !lowerLeg.empty() && !foot.empty() && !toe.empty();
	}
};

/** 骨の名前から左右の脚を探す。見つからなければ空のまま返す。 */
inline void FindLegBones(
	const std::vector<std::string>& boneNames,
	LegBoneNames& left,
	LegBoneNames& right)
{
	const auto normalize = [](std::string value) {
		std::string result;
		result.reserve(value.size());
		for (const char c : value)
		{
			if (c == '_' || c == '-' || c == ' ' || c == ':')
				continue;
			result.push_back(c >= 'A' && c <= 'Z' ? static_cast<char>(c - 'A' + 'a') : c);
		}
		return result;
	};
	for (const std::string& name : boneNames)
	{
		const std::string key = normalize(name);
		const bool isLeft = key.find("left") != std::string::npos || key.find(".l") != std::string::npos;
		const bool isRight = key.find("right") != std::string::npos || key.find(".r") != std::string::npos;
		if (!isLeft && !isRight)
			continue;
		LegBoneNames& leg = isLeft ? left : right;
		if (key.find("upleg") != std::string::npos || key.find("upperleg") != std::string::npos ||
			key.find("thigh") != std::string::npos)
		{
			if (leg.upperLeg.empty()) leg.upperLeg = name;
		}
		else if (key.find("toebase") != std::string::npos || key.find("toe") != std::string::npos)
		{
			if (leg.toe.empty()) leg.toe = name;
		}
		else if (key.find("foot") != std::string::npos || key.find("ankle") != std::string::npos)
		{
			if (leg.foot.empty()) leg.foot = name;
		}
		else if (key.find("leg") != std::string::npos || key.find("calf") != std::string::npos ||
			key.find("shin") != std::string::npos || key.find("knee") != std::string::npos)
		{
			if (leg.lowerLeg.empty()) leg.lowerLeg = name;
		}
	}
}

} // namespace FootLock
