#include	<iostream>
#include	"CAnimationMesh.h"
#include	"utility.h"

void CAnimationMesh::SetCurentAnimation(aiAnimation * currentanimation) {
	m_CurrentAnimation = currentanimation;
}

// ノードツリー表示(debug用)
static void DispNodeTree(CTreeNode<std::string>* ptree) 
{
	std::cout << ptree->m_nodedata << std::endl;

	for (unsigned int n = 0; n < ptree->m_children.size(); n++)
	{
		DispNodeTree(ptree->m_children[n].get());
	}
}

void CAnimationMesh::Draw()
{
	// メッシュ描画
	m_StaticMeshRenderer.Draw();
}


void CAnimationMesh::Load(std::string filename, std::string texturedirectory) 
{
	// メッシュ読み込み
	CStaticMesh::Load(filename, texturedirectory);

	// アニメーションデータ(ASSIMP用）
	std::unordered_map<std::string, GM31::GE::myAssimp::BONE> assimp_BoneDictionary{};	// 20240714 DX化

	// ボーン辞書取得（ボーン名をキーにしてボーン情報が取れる）
	assimp_BoneDictionary = GM31::GE::myAssimp::GetBoneDictionary();					// 20240714 DX化

	for (auto& asimpbone : assimp_BoneDictionary) {										// 20240714 DX化
		BONE dxbone;																	// 20240714 DX化	

		dxbone.meshname = asimpbone.second.meshname;									// 20240714 DX化
		dxbone.armaturename = asimpbone.second.armaturename;							// 20240714 DX化
		dxbone.bonename = asimpbone.second.bonename;									// 20240714 DX化
		dxbone.idx = asimpbone.second.idx;												// 20240714 DX化

		dxbone.OffsetMatrix = utility::aiMtxToDxMtx(asimpbone.second.OffsetMatrix);
		dxbone.AnimationMatrix = Matrix4x4::Identity;										// 20240714 DX化
		dxbone.Matrix = Matrix4x4::Identity;												// 20240714 DX化

		dxbone.weights.clear();															// 20240714 DX化
		for (auto& asimpweight : asimpbone.second.weights)								// 20240714 DX化	
		{
			WEIGHT dxweight;															// 20240714 DX化			
			dxweight.bonename = asimpweight.bonename;									// 20240714 DX化
			dxweight.meshname = asimpweight.meshname;									// 20240714 DX化
			dxweight.vertexindex = asimpweight.vertexindex;								// 20240714 DX化
			dxweight.weight = asimpweight.weight;										// 20240714 DX化
			dxbone.weights.push_back(dxweight);											// 20240714 DX化		
		}																				// 20240714 DX化

		m_BoneDictionary[asimpbone.first] = dxbone;										// 20240714 DX化
	}																	

	// ボーン名ツリー取得
	m_AssimpNodeNameTree = GM31::GE::myAssimp::GetBoneNameTree();

	// レンダラ初期化
	m_StaticMeshRenderer.Init(*this);

}

// 階層構造を考慮したボーンコンビネーション行列を更新
void CAnimationMesh::UpdateBoneMatrix(
	CTreeNode<std::string>* ptree, 
	Matrix4x4 matrix)														// 20240714 DX化	
{
	// ノード名からボーン辞書を使ってボーン情報を取得
	BONE* bone = &m_BoneDictionary[ptree->m_nodedata];						// 20240714 DX化		

	Matrix4x4 bonecombination;												// 20240714 DX化；

	// ボーンオフセット行列×ボーンアニメメーション行列×逆ボーンオフセット行列
	bonecombination = bone->OffsetMatrix * bone->AnimationMatrix * matrix;	// 20240714 DX化
	bone->Matrix = bonecombination;											// 20240714 DX化

	// 自分の姿勢を表す行列を作成
	Matrix4x4 mybonemtx;													// 20240714 DX化
	mybonemtx = bone->AnimationMatrix * matrix;								// 20240714 DX化
	// 子ノードに対して再帰的に処理											// 20240714 DX化
	for (unsigned int n = 0; n < ptree->m_children.size(); n++)				// 20240714 DX化
	{																		// 20240714 DX化
		UpdateBoneMatrix(ptree->m_children[n].get(), mybonemtx);			// 20240714 DX化
	}																		// 20240714 DX化
}

// ローカルポーズ生成
void CAnimationMesh::BuildLocalPoseMap(
	const aiAnimation* animationdata,
	int& CurrentFrame,
	std::unordered_map<std::string, SRTQ>& localposemap)
{
	// アニメーションデータ取得
	const aiAnimation* animation = animationdata;

	// ボーン数分ループしてボーン行列を作成
	for (unsigned int c = 0; c < animation->mNumChannels; c++)
	{
		aiNodeAnim* nodeAnim = animation->mChannels[c];

		int f;

		f = CurrentFrame % nodeAnim->mNumRotationKeys;				//簡易実装
		aiQuaternion rot = nodeAnim->mRotationKeys[f].mValue;

		f = CurrentFrame % nodeAnim->mNumPositionKeys;				//簡易実装
		aiVector3D pos = nodeAnim->mPositionKeys[f].mValue;

		// assimp SRT=>DX版　SRT
		Vector3 s = { 1.0f,1.0f,1.0f };		// 20240714 DX化
		Vector3 t = { pos.x,pos.y,pos.z };	// 20240714 DX化
		Quaternion r{};						// 20240714 DX化

		r.x = rot.x;						// 20240714 DX化
		r.y = rot.y;						// 20240714 DX化
		r.z = rot.z;						// 20240714 DX化
		r.w = rot.w;						// 20240714 DX化

		SRTQ srtq;
		srtq.scale = s;
		srtq.pos = t;
		srtq.rot = Vector3(0, 0, 0);
		srtq.quat = r;

		localposemap[nodeAnim->mNodeName.C_Str()] = srtq;
	}
}

// アニメーションの更新
void CAnimationMesh::Update(BoneCombMatrix& bonecombarray,int& CurrentFrame)
{
	// アニメーションデータ取得
	aiAnimation* animation = m_CurrentAnimation;

	// ローカルポーズを生成
	std::unordered_map<std::string, SRTQ> localpose;
	BuildLocalPoseMap(
		m_CurrentAnimation,
		CurrentFrame,
		localpose);

	// localpose の中身を 1 件ずつ取り出す
	for (auto& pair : localpose) {
		// map の「キー（ボーン名）」と「値（SRTQデータ）」を明示的に取り出す
		const std::string& bonename = pair.first;   // ボーンの名前
		SRTQ& srtq = pair.second;                   // 位置・回転・スケールの情報

		// ノード名からボーン辞書を使ってassimpのボーン情報を取得
		BONE* bone = &m_BoneDictionary[bonename];

		Matrix4x4 scalemtx = Matrix4x4::CreateScale(srtq.scale);
		Matrix4x4 rotmtx = Matrix4x4::CreateFromQuaternion(srtq.quat);
		Matrix4x4 transmtx = Matrix4x4::CreateTranslation(srtq.pos);

		// ローカル座標からボーンのアニメーション行列を作成
		bone->AnimationMatrix = scalemtx * rotmtx * transmtx;
	}


	UpdateBoneMatrix(&m_AssimpNodeNameTree, Matrix4x4::Identity);		// 20240714 DX化	

	// ボーンコンビネーション行列の配列をセット
	for (const auto& bone : m_BoneDictionary)
	{
		bonecombarray.ConstantBufferMemory.BoneCombMtx[bone.second.idx] = bone.second.Matrix.Transpose();	// 20240714 DX化
	}
}
