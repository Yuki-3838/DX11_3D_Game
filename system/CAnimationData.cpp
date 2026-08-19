#include	<cassert>
#include	<iostream>
#include	"CAnimationData.h"

const aiScene* CAnimationData::LoadAnimation(const std::string filename, const std::string name)
{
	// Assimp::Importer owns the returned aiScene. Keep one importer alive per
	// animation so loading another clip cannot invalidate earlier clips.
	auto importer = std::make_unique<Assimp::Importer>();
	const aiScene* scene = importer->ReadFile(
		filename.c_str(),
		aiProcess_ConvertToLeftHanded);
	m_Animation[name] = scene;
	m_animationImporters.push_back(std::move(importer));
	assert(m_Animation[name]);

	if (m_Animation[name] == nullptr) {
		std::cout << " animation load error " << filename  << " "
			<< m_animationImporters.back()->GetErrorString();
	}

	return m_Animation[name];
}

// 指定した名前のアニメーションデータを取得する
aiAnimation* CAnimationData::GetAnimation(const char* name, int idx) {
	const auto it = m_Animation.find(name);
	if (it == m_Animation.end() || it->second == nullptr ||
		idx < 0 || static_cast<unsigned int>(idx) >= it->second->mNumAnimations)
	{
		std::cout << " animation not found " << name << " index " << idx << std::endl;
		return nullptr;
	}

	return it->second->mAnimations[idx];
}
