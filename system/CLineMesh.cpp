#include	"CLineMesh.h"

// èâä˙âª
void CLineMesh::Init(
	Vector3 p1,
	Vector3 direction,
	float length)
{
	m_p1 = p1;
	m_direction = direction;
	m_length = length;

	VERTEX_3D v1{};
	v1.Position = p1;
	v1.Diffuse = m_color;
	m_vertices.emplace_back(v1);

	VERTEX_3D v2{};

	direction.Normalize();

	v2.Position = p1 + direction * length;
	v2.Diffuse = m_color;
	m_vertices.emplace_back(v2);

	m_indices.emplace_back(0);
	m_indices.emplace_back(1);
}

void CLineMesh::SetPoints(Vector3 p1, Vector3 p2)
{
	m_vertices[0].Position = p1;
	m_vertices[1].Position = p2;
}
