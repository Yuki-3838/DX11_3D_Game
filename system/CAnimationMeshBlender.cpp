#include	<iostream>
#include	"CAnimationMeshBlender.h"
#include    "transform.h"

void CAnimationMeshBlender::SetFromAnimation(
	const aiAnimation* animatiodata,
	bool loop, 
	float speed) 
{
	m_fromblendstate.animatiodata = animatiodata;
	m_fromblendstate.loop = loop;
	m_fromblendstate.speed = speed;
}

void CAnimationMeshBlender::SetToAnimation(
	const aiAnimation* animatiodata,
	bool loop, 
	float speed) 
{
	m_toblendstate.animatiodata = animatiodata;
	m_toblendstate.loop = loop;
	m_toblendstate.speed = speed;
}

// ローカルポーズのブレンド
void CAnimationMeshBlender::BlendLocalPose(
    const std::unordered_map<std::string, SRTQ>& localposefrom,
    const std::unordered_map<std::string, SRTQ>& localposeto,
    float rate,
    std::unordered_map<std::string, SRTQ>& blendedlocalpose) {

    blendedlocalpose.clear();
    blendedlocalpose.reserve(std::max(localposefrom.size(), localposeto.size()));

    // 1) まず toPose のキーを基準にブレンド（最終姿勢に存在するボーンを優先）
    for (const auto& [bone, toSrtq] : localposeto) {
        auto itFrom = localposefrom.find(bone);
        const SRTQ& fromSrtq = (itFrom != localposefrom.end()) ? itFrom->second : SRTQ();

        SRTQ blended{};
        blended.pos = Vector3::Lerp(fromSrtq.pos, toSrtq.pos, rate);
        blended.scale = Vector3::Lerp(fromSrtq.scale, toSrtq.scale, rate);
        blended.quat = Quaternion::Slerp(fromSrtq.quat, toSrtq.quat, rate); 

        blendedlocalpose.emplace(bone, blended);
    }

    // 2) from にのみ存在するキーも取りこぼさない（to に無いなら to=Identity とみなす）
    for (const auto& [bone, fromSrtq] : localposefrom) {
        if (blendedlocalpose.find(bone) != blendedlocalpose.end()) continue; // 既に処理済み
        const SRTQ& toSrtq = SRTQ();

        SRTQ blended{};
        blended.pos = Vector3::Lerp(fromSrtq.pos, toSrtq.pos, rate);
        blended.scale = Vector3::Lerp(fromSrtq.scale, toSrtq.scale, rate);
        blended.quat = Quaternion::Slerp(fromSrtq.quat, toSrtq.quat, rate);

        blendedlocalpose.emplace(bone, blended);
    }
}

void CAnimationMeshBlender::UpdateBlended(BoneCombMatrix& bonecombarray, int& CurrentFrame)
{
    // 1) Aローカル姿勢マップを構築
    std::unordered_map<std::string, SRTQ> localposefrom;
    std::unordered_map<std::string, SRTQ> localposeto;

    if (m_toblendstate.animatiodata != nullptr) {
        BuildLocalPoseMap(m_fromblendstate.animatiodata, CurrentFrame, localposefrom);
    }

    if (m_toblendstate.animatiodata != nullptr) {
        BuildLocalPoseMap(m_toblendstate.animatiodata, CurrentFrame, localposeto);
    }

    // ブレンドしたローカルポーズを生成
    std::unordered_map<std::string, SRTQ> blendedlocalpose;
    BlendLocalPose(localposefrom, localposeto, m_blendrate, blendedlocalpose);

    // 2)　グローバルポーズを生成する
    for (auto& [bonename, srtq] : blendedlocalpose) {
        // ノード名からボーン辞書を使ってassimpのボーン情報を取得
        BONE* bone = &m_BoneDictionary[bonename];

        Matrix4x4 scalemtx = Matrix4x4::CreateScale(srtq.scale);
        Matrix4x4 rotmtx = Matrix4x4::CreateFromQuaternion(srtq.quat);
        Matrix4x4 transmtx = Matrix4x4::CreateTranslation(srtq.pos);

        bone->AnimationMatrix = scalemtx * rotmtx * transmtx;
    }

    // ボーンコンビネーション行列をすべて再帰を使用して更新する
    UpdateBoneMatrix(&m_AssimpNodeNameTree, Matrix4x4::Identity);

    // ボーンコンビネーション行列の配列をセット
    for (const auto& bone : m_BoneDictionary)
    {
        bonecombarray.ConstantBufferMemory.BoneCombMtx[bone.second.idx] = bone.second.Matrix.Transpose();
    }
}
