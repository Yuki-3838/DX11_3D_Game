#pragma once
#include    "CommonTypes.h"
#include	<string>
#include	<filesystem>
#include	<assimp/scene.h>
#include	<directxmath.h>

namespace utility
{
	namespace fs = std::filesystem;

	std::string wide_to_multi_winapi(std::wstring const& src);
	std::wstring utf8_to_wide_winapi(std::string const& src);
	std::string utf8_to_multi_winapi(std::string const& src);

	Matrix4x4 aiMtxToDxMtx(aiMatrix4x4 asimpmtx);

	Matrix4x4 CaliculateBillBoardMtx(Matrix4x4 mtxView);

	// ターゲットの方向を向くクオータニオンを作成する関数(上固定なし)
	Quaternion CreateTargetQuaternion(
		const Vector3 start,
		const Vector3 end);

	// ターゲットの方向を向くクオータニオンを作成する関数(上固定あり)
	Quaternion CreateLookatQuaternion(
		Vector3 start,
		Vector3 end,
		Vector3 up,
		Matrix4x4& mtx);

	Vector3 UnprojectSimpleMath(
		const Vector3& screenPos,
		float viewportX, float viewportY,
		float viewportWidth, float viewportHeight,
		float minZ, float maxZ,
		const Matrix4x4& projection,
		const Matrix4x4& view,
		const Matrix4x4& world);

	std::vector<Vector3> GeneratePointsInsideCone(
		float radius,					// 半径
		float height,					// 高さ
		unsigned int num				// 個数
	);

	std::vector<Vector3> GenerateRandomPointInCircle(
		float radius,						// 半径	
		unsigned int num);					// 発生させる個数

	std::string WideToUtf8(const std::wstring& ws);
	std::string PathToUtf8String(const fs::path& p);
}