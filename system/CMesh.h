#pragma once
#include	<vector>
#include	"renderer.h"

class CMesh {
protected:
	std::vector<VERTEX_3D>	m_vertices;		// 頂点座標群
	std::vector<uint32_t>	m_indices;		// インデックスデータ群
public:
	virtual ~CMesh() {

	}
	// 頂点データ取得
	const std::vector<VERTEX_3D>& GetVertices() const{
		return m_vertices;
	}

	// インデックスデータ取得
	const std::vector<unsigned int>& GetIndices() const {
		return m_indices;
	}
};

