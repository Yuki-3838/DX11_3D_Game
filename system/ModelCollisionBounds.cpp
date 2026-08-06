#include "collision.h"

#include <cmath>

namespace GM31::GE::Collision
{
	BoundingBoxAABB BuildLocalAABBFromVertices(const std::vector<Vector3>& vertices)
	{
		if (vertices.empty())
			return { Vector3(0.0f, 0.0f, 0.0f), Vector3(0.0f, 0.0f, 0.0f) };

		// The model's exact local bounds are the component-wise minimum and
		// maximum over every imported vertex. Do not include an artificial
		// origin point: doing so enlarges models whose geometry is offset from 0.
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
		// Project the three OBB half-axes onto world X/Y/Z. For each world axis,
		// its AABB half-size is:
		//   |axisX| * halfX + |axisY| * halfY + |axisZ| * halfZ
		// This produces the smallest world-aligned box containing the OBB.
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

		// SRT::GetMatrix uses row-vector order. Therefore rows 1, 2 and 3 are
		// transformed local X, Y and Z unit vectors. Their lengths contain the
		// effective scale (including non-uniform or negative scale), while their
		// normalized values are the OBB directions after rotation.
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
		// Transforming the local center with the full matrix also handles model
		// origins and SRT pivots that are not at the mesh's geometric center.
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
