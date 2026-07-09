#pragma once

#include	"CommonTypes.h"
#include	"CMesh.h"

class CLineMesh : public CMesh
{
	Vector3 m_p1{};
	Vector3 m_direction{};
	Color m_color = { 1,1,1,1 };
	float m_length = { 100 };
public:
	void Init(
		Vector3 p1,
		Vector3 direction,
		float length);

	void SetPoints(Vector3 p1, Vector3 p2);

	void Clear() {
		m_vertices.clear();
		m_indices.clear();
	}
};