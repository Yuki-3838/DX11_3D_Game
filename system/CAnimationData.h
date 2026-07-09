#pragma once
#include	<assimp\Importer.hpp>
#include	<assimp\scene.h>
#include	<assimp\postprocess.h>
#include	<assimp/cimport.h>
#include	<unordered_map>
#include	"NonCopyable.h"

class CAnimationData : NonCopyable{
	// このアニメーションのパス名
	std::string m_filename;

	// アニメーションデータ格納辞書（キーはモーション名）
	std::unordered_map<std::string, const aiScene*> m_Animation;

	// importer解放される際　シーンも解放されるのでメンバ変数にしてる
	Assimp::Importer m_importer;	// ボーン情報
public:
	// アニメーションデータ読み込み
	const aiScene* LoadAnimation(const std::string filename, const std::string name);

	// 指定した名前のアニメーションを取得する
	aiAnimation* GetAnimation(const char* name, int idx);

	// アニメーションデータが格納されているａｉＳｃｅｎｅを獲得する
	const aiScene* GetAiScene(std::string name) 
	{ 
		return m_Animation[name]; 
	}
};