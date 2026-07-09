#pragma once

#include	"CAnimationMesh.h"
#include    "transform.h"

class CAnimationMeshBlender : public CAnimationMesh
{
public:

    // モーションブレンドステート
    struct blendanimstate {
        const aiAnimation* animatiodata = nullptr;  // アニメーションデータ
        double             timeSec = 0.0;           // このアニメーションの再生位置
        float              speed = 1.0f;            // スピード
        bool               loop = true;             // ループ再生するか否か
    };

    void SetFromAnimation(
        const aiAnimation* animatiodata,
        bool loop=true,
        float = 1.0f);

    void SetToAnimation(
        const aiAnimation* animatiodata,
        bool loop = true, 
        float = 1.0f);

    void UpdateBlended(
        BoneCombMatrix& outBoneComb, 
        int& CurrentFrame);

    void SetBlendrate(float rate) {
        m_blendrate = rate;
    }

private:

    // ローカルポーズのブレンド
    void BlendLocalPose(
        const std::unordered_map<std::string, SRTQ>& localposefrom,
        const std::unordered_map<std::string, SRTQ>& localposeto,
        float rate,
        std::unordered_map<std::string, SRTQ>& blendedlocalpose);

	// cross fade用
	float	m_blendrate = 1.0f;    // ブレンド割合 from * (1-m_blendarate) + to*m_blendarate

	// ブレンドするアニメーションの設定
    blendanimstate    m_fromblendstate;
    blendanimstate    m_toblendstate;

};
