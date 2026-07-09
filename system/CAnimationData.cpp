#include	<cassert>
#include	<iostream>
#include	"CAnimationData.h"

const aiScene* CAnimationData::LoadAnimation(const std::string filename, const std::string name)
{
	// シーン情報を構築
	m_Animation[name] = m_importer.ReadFile(
		filename.c_str(),
		aiProcess_ConvertToLeftHanded);
	assert(m_Animation[name]);

	if (m_Animation[name] == nullptr) {
		std::cout << " animation load error " << filename  << " " << m_importer.GetErrorString();
	}

	return m_Animation[name];
}

// 指定した名前のアニメーションデータを取得する
aiAnimation* CAnimationData::GetAnimation(const char* name, int idx) {

	aiAnimation* animation = m_Animation[name]->mAnimations[idx];

	return animation;
}
