#pragma once
#include <memory>
#include <string>
#include <vector>
#include	<assimp\Importer.hpp>
#include	<assimp\scene.h>
#include	<assimp\postprocess.h>
#include	<assimp/cimport.h>
#include	<unordered_map>
#include	"NonCopyable.h"

/**
 * @brief モーション(クリップ)を名前で持つ。
 *
 * クリップのファイル(FBX)を Assimp で読むと、モデルの頂点やテクスチャの情報まで丸ごと読まれ、
 * Debug 版では1本あたり約0.1秒かかっていた。ゲームが使うのはその中のアニメーション(aiAnimation)だけなので、
 * それだけを取り出して自前で持ち、読み込みの保存ファイル(ModelCache.h)にも書いておく。
 * 次からは保存ファイルを読むだけで済む(Assimp を通らない)。
 */
class CAnimationData : NonCopyable{
	// モーション名 → そのファイルに入っていたアニメーション(1つのファイルに複数入っていることがある)
	std::unordered_map<std::string, std::vector<aiAnimation*>> m_Animation;
	// 読んだアニメーションの持ち主。同じ名前で読み直しても(攻撃のタイムラインエディターのクリップ差し替え)、
	// 前のものを使っている途中の処理があるので、このデータが消えるまで捨てない。
	std::vector<std::unique_ptr<aiAnimation>> m_ownedAnimations;
public:
	// アニメーションデータ読み込み。アニメーションが1つ以上読めたら true。
	bool LoadAnimation(const std::string& filename, const std::string& name);

	// 指定した名前のアニメーションを取得する
	aiAnimation* GetAnimation(const char* name, int idx);

	// その名前のアニメーションを読んであるか
	bool HasAnimation(const std::string& name) const
	{
		const auto it = m_Animation.find(name);
		return it != m_Animation.end() && !it->second.empty();
	}
};
