#include "collision.h"

#include <cmath>

namespace GM31::GE::Collision
{
	BoundingBoxAABB BuildLocalAABBFromVertices(const std::vector<Vector3>& vertices)
	{
		if (vertices.empty())
			return { Vector3(0.0f, 0.0f, 0.0f), Vector3(0.0f, 0.0f, 0.0f) };

		// 読み込んだ全頂点から各成分の最小値と最大値を求め、モデル本来のローカル境界を作る。
		// 人為的な原点を含めないことで、原点から離れたモデルの判定箱が不必要に大きくならない。
		Vector3 minimum = vertices.front();
		Vector3 maximum = vertices.front();
		for (const Vector3& vertex : vertices)
		{
			minimum = Vector3::Min(minimum, vertex);
			maximum = Vector3::Max(maximum, vertex);
		}
		return { minimum, maximum };
	}

	BoundingBoxAABB BuildWorldAABBFromOBB(const BoundingBoxOBB& obb)
	{
		// OBBの3本の半軸をワールドのX/Y/Z軸へ投影し、各軸のAABB半サイズを求める。
		//   |axisX| * halfX + |axisY| * halfY + |axisZ| * halfZ
		// これにより、OBBを内包する最小のワールド軸平行箱が得られる。
		const Vector3 half(
			std::abs(obb.axisX.x) * obb.lengthx * 0.5f +
			std::abs(obb.axisY.x) * obb.lengthy * 0.5f +
			std::abs(obb.axisZ.x) * obb.lengthz * 0.5f,
			std::abs(obb.axisX.y) * obb.lengthx * 0.5f +
			std::abs(obb.axisY.y) * obb.lengthy * 0.5f +
			std::abs(obb.axisZ.y) * obb.lengthz * 0.5f,
			std::abs(obb.axisX.z) * obb.lengthx * 0.5f +
			std::abs(obb.axisY.z) * obb.lengthy * 0.5f +
			std::abs(obb.axisZ.z) * obb.lengthz * 0.5f);
		return { obb.worldcenter - half, obb.worldcenter + half };
	}

	BoundingBoxOBB BuildWorldOBBFromLocalAABB(
		const BoundingBoxAABB& localBounds,
		const SRT& transform)
	{
		const Vector3 localCenter = (localBounds.min + localBounds.max) * 0.5f;
		const Vector3 localSize = localBounds.max - localBounds.min;
		const Matrix4x4 world = transform.GetMatrix();

		// SRT::GetMatrixは行ベクトル順で計算するため、1～3行目は変換後のローカルX/Y/Z単位ベクトルになる。
		// 各行の長さから実効倍率を求め、正規化した値を回転後のOBB軸として使用する。
		Vector3 scaledAxisX(world._11, world._12, world._13);
		Vector3 scaledAxisY(world._21, world._22, world._23);
		Vector3 scaledAxisZ(world._31, world._32, world._33);
		const float scaleX = scaledAxisX.Length();
		const float scaleY = scaledAxisY.Length();
		const float scaleZ = scaledAxisZ.Length();

		const Vector3 axisX = scaleX > 0.000001f
			? scaledAxisX / scaleX : Vector3(1.0f, 0.0f, 0.0f);
		const Vector3 axisY = scaleY > 0.000001f
			? scaledAxisY / scaleY : Vector3(0.0f, 1.0f, 0.0f);
		const Vector3 axisZ = scaleZ > 0.000001f
			? scaledAxisZ / scaleZ : Vector3(0.0f, 0.0f, 1.0f);

		BoundingBoxOBB result{};
		result.center = localCenter;
		// ローカル中心を行列全体で変換することで、メッシュの幾何中心と異なる原点やSRTピボットにも対応する。
		result.worldcenter = Vector3::Transform(localCenter, world);
		result.axisX = axisX;
		result.axisY = axisY;
		result.axisZ = axisZ;
		result.lengthx = std::abs(localSize.x) * scaleX;
		result.lengthy = std::abs(localSize.y) * scaleY;
		result.lengthz = std::abs(localSize.z) * scaleZ;

		const BoundingBoxAABB enclosingAabb = BuildWorldAABBFromOBB(result);
		result.min = enclosingAabb.min;
		result.max = enclosingAabb.max;
		return result;
	}
}
