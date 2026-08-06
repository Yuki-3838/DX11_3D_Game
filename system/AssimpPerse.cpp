#include	<vector>
#include	<utility>
#include	<iostream>
#include	<unordered_map>
#include	<cassert>
#include	"CTexture.h"
#include	"AssimpPerse.h"
#include	"CTreeNode.h"
#include	"utility.h"

#pragma comment(lib, "assimp-vc143-mtd.lib")

namespace GM31 {namespace GE {namespace {}
namespace myAssimp{
	std::vector<std::vector<VERTEX>> g_vertices{};				// 鬆らせ繝・・繧ｿ・医Γ繝・す繝･蜊倅ｽ搾ｼ・

	std::vector<std::vector<unsigned int>> g_indices{};			// 繧､繝ｳ繝・ャ繧ｯ繧ｹ繝・・繧ｿ・医Γ繝・す繝･蜊倅ｽ搾ｼ・

	std::vector<SUBSET> g_subsets{};							// 繧ｵ繝悶そ繝・ヨ諠・ｱ

	std::vector<MATERIAL> g_materials{};						// 繝槭ユ繝ｪ繧｢繝ｫ

	std::vector<std::unique_ptr<CTexture>> g_diffuseTextures;	// 繝・ぅ繝輔Η繝ｼ繧ｺ繝・け繧ｹ繝√Ε鄒､

	std::unordered_map<std::string, BONE> g_BoneDictionary;		// 繝懊・繝ｳ霎樊嶌・医く繝ｼ・壹・繝ｼ繝ｳ蜷搾ｼ・	
	std::unordered_map<std::string, aiMatrix4x4> g_NodeLocalMatrices;
	
	std::vector<std::vector<BONE>>	g_BonesPerMeshes;			// 繝｡繝・す繝･蜊倅ｽ阪〒繝懊・繝ｳ諠・ｱ繧帝寔繧√◆繧ゅ・

	CTreeNode<std::string>	g_AssimpNodeNameTree;				// assimp繝弱・繝牙錐繝・Μ繝ｼ

	std::unordered_map<std::string, aiMatrix4x4> GetNodeLocalMatrices()
	{
		return g_NodeLocalMatrices;
	}

	// 繝弱・繝牙錐繝・Μ繝ｼ繧堤函謌舌☆繧・
	void CreateNodeTree(aiNode* node, CTreeNode<std::string>* ptree) {

		ptree->m_nodedata = std::string(node->mName.C_Str());
		g_NodeLocalMatrices[ptree->m_nodedata] = node->mTransformation;
//		std::cout << node->mName.C_Str() << std::endl;

		for (unsigned int n = 0; n < node->mNumChildren; n++)
		{
			std::unique_ptr<CTreeNode<std::string>> pchild = std::make_unique<CTreeNode<std::string>>();
			pchild->m_parent = ptree;
			ptree->Addchild(std::move(pchild));
			CreateNodeTree(node->mChildren[n], ptree->m_children[n].get());
		}
	}

	CTreeNode<std::string> GetBoneNameTree() 
	{
		return std::move(g_AssimpNodeNameTree);
	}

	// 繝懊・繝ｳ霎樊嶌繧定ｿ斐☆	
	std::unordered_map<std::string, BONE> GetBoneDictionary()
	{
		return g_BoneDictionary;
	}

	// 遨ｺ縺ｮ繝懊・繝ｳ霎樊嶌・医く繝ｼ縺ｯ繝懊・繝ｳ蜷搾ｼ峨ｒ菴懈・縺吶ｋ・医ヮ繝ｼ繝峨ｒ蜀榊ｸｰ縺ｧ霎ｿ繧顔ｩｺ縺ｮ霎樊嶌繧剃ｽ懈・縺吶ｋ・・
	void CreateEmptyBoneDictionary(aiNode* node)
	{
		BONE bone{};

		// 繝懊・繝ｳ蜷阪〒蜿ら・縺ｧ縺阪ｋ繧医≧縺ｫ遨ｺ縺ｮ繝懊・繝ｳ諠・ｱ繧偵そ繝・ヨ縺吶ｋ
		g_BoneDictionary[node->mName.C_Str()] = bone;

		std::cout << node->mName.C_Str() << std::endl;

		for (unsigned int n = 0; n < node->mNumChildren; n++)
		{
			CreateEmptyBoneDictionary(node->mChildren[n]);
		}
	}

	// 繧ｵ繝悶そ繝・ヨ縺ｫ邏舌▼縺・※縺・ｋ繝懊・繝ｳ諠・ｱ繧貞叙蠕励☆繧・
	std::vector<BONE> GetBonesPerMesh(const aiMesh* mesh)
	{
		std::vector<BONE> bones;		// 縺薙・繧ｵ繝悶そ繝・ヨ繝｡繝・す繝･縺ｧ菴ｿ逕ｨ縺輔ｌ縺ｦ縺・ｋ繝懊・繝ｳ繧ｳ繝ｳ繝・リ

		// 繝懊・繝ｳ謨ｰ蛻・Ν繝ｼ繝・
		for (unsigned int bidx = 0; bidx < mesh->mNumBones; bidx++) {

			BONE bone{};

			// 繝懊・繝ｳ蜷榊叙蠕・
			bone.bonename = std::string(mesh->mBones[bidx]->mName.C_Str());

			// 繝｡繝・す繝･繝弱・繝牙錐
// ts 20260531			bone.meshname = std::string(mesh->mBones[bidx]->mNode->mName.C_Str());


			// 繧｢繝ｼ繝槭メ繝･繧｢繝弱・繝牙錐
//	ts 20260531		bone.armaturename = std::string(mesh->mBones[bidx]->mArmature->mName.C_Str());

			// 繝・ヰ繝・げ逕ｨ
			std::cout
				<< "(" << bidx << ")"
				<< bone.bonename
				<< "(" << bone.meshname << ")"
				<< "(" << bone.armaturename << ")"
				<< std::endl;

			// 繝懊・繝ｳ繧ｪ繝輔そ繝・ヨ陦悟・蜿門ｾ・
			bone.OffsetMatrix = mesh->mBones[bidx]->mOffsetMatrix;

			// 繧ｦ繧ｧ繧､繝域ュ蝣ｱ謚ｽ蜃ｺ
			bone.weights.clear();
			for (unsigned int widx = 0; widx < mesh->mBones[bidx]->mNumWeights; widx++) {

				WEIGHT w;
				w.meshname = bone.meshname;										// 繝｡繝・す繝･蜷・
				w.bonename = bone.bonename;										// 繝懊・繝ｳ蜷・

				w.weight = mesh->mBones[bidx]->mWeights[widx].mWeight;			// 驥阪∩
				w.vertexindex = mesh->mBones[bidx]->mWeights[widx].mVertexId;	// 鬆らせ繧､繝ｳ繝・ャ繧ｯ繧ｹ
				bone.weights.emplace_back(w);
			}

			// 繧ｳ繝ｳ繝・リ縺ｫ逋ｻ骭ｲ
			bones.emplace_back(bone);

			// 繝懊・繝ｳ霎樊嶌縺ｫ繧ょ渚譏縺輔○繧・
			g_BoneDictionary[mesh->mBones[bidx]->mName.C_Str()].OffsetMatrix = mesh->mBones[bidx]->mOffsetMatrix;
		}

		return bones;
	}

	// 繝懊・繝ｳ蜷阪√・繝ｼ繝ｳ繧､繝ｳ繝・ャ繧ｯ繧ｹ縲√・繝ｼ繝ｳ繧ｦ繧ｧ繧､繝医ｒ鬆らせ縺ｫ繧ｻ繝・ヨ縺吶ｋ・・0231225霑ｽ蜉・・
	void SetBoneDataToVertices() {

		// 繝懊・繝ｳ繧､繝ｳ繝・ャ繧ｯ繧ｹ繧貞・譛溷喧
		for (auto& vtbl : g_vertices) {
			for (auto& v : vtbl) {
				v.bonecnt = 0;
				for (int b = 0; b < 4; b++) {
					v.BoneIndex[b] = -1;
					v.BoneWeight[b] = 0.0f;
				}
			}
		}

		// 繝｡繝・す繝･豈弱・繝懊・繝ｳ繧ｳ繝ｳ繝・リ
		int subsetid = 0;
		for (auto& bones : g_BonesPerMeshes) {

			// 縺薙・繧ｹ繧ｿ繝・ぅ繝・け繝｡繝・す繝･蜀・・鬆らせ繝・・繧ｿ縺ｮ繧ｹ繧ｿ繝ｼ繝井ｽ咲ｽｮ繧貞叙蠕・
//			int vertexbase = g_subsets[subsetid].VertexBase;

			// 縺薙・繧ｵ繝悶そ繝・ヨ蜀・・繝懊・繝ｳ繧偵・縺ｨ縺､縺･縺､蜿悶ｊ蜃ｺ縺・
			for (auto& bone : bones)
			{
				for (auto& w : bone.weights) {
					int& idx = g_vertices[subsetid][w.vertexindex].bonecnt;
					if (idx >= 4) {
						continue;
					}

					g_vertices[subsetid][w.vertexindex].BoneWeight[idx] = w.weight;	// weight蛟､繧偵そ繝・ヨ
					g_vertices[subsetid][w.vertexindex].BoneIndex[idx] = g_BoneDictionary[w.bonename].idx;

					//繝懊・繝ｳ縺ｮ驟榊・逡ｪ蜿ｷ繧偵そ繝・ヨ
					idx++;
					assert(idx <= 4);
				}
			}
			subsetid++;				// 谺｡縺ｮ繝｡繝・す繝･縺ｸ
		}
	}

	// 繝懊・繝ｳ諠・ｱ繧貞叙蠕励☆繧具ｼ医ヮ繝ｼ繝峨ｒ蜀榊ｸｰ縺ｧ霎ｿ繧翫・繝ｼ繝ｳ諠・ｱ繧貞叙蠕励☆繧具ｼ・
	void GetBone(const aiScene* pScene)
	{
		// 遨ｺ縺ｮ繝懊・繝ｳ霎樊嶌繧剃ｽ懈・縺吶ｋ・医く繝ｼ・医・繝ｼ繝ｳ蜷搾ｼ峨□縺代・蜿悶ｊ蜃ｺ縺暦ｼ・
		CreateEmptyBoneDictionary(pScene->mRootNode);

		// 繝懊・繝ｳ縺ｮ驟榊・菴咲ｽｮ・医う繝ｳ繝・ャ繧ｯ繧ｹ蛟､・峨ｒ譬ｼ邏阪☆繧・
		unsigned int num = 0;						
		for (auto& data : g_BoneDictionary) 
		{											
			data.second.idx = num;					
			num++;									
		}					

		// 繝｡繝・す繝･謨ｰ蛻・Ν繝ｼ繝・
		for (unsigned int m = 0; m < pScene->mNumMeshes; m++)
		{
			aiMesh* mesh = pScene->mMeshes[m];

			// 繧ｵ繝悶そ繝・ヨ縺ｫ邏舌▼縺・※縺・ｋ繝懊・繝ｳ諠・ｱ繧貞叙蠕励☆繧・
			std::vector<BONE> BonesPerMesh = GetBonesPerMesh(mesh);
			g_BonesPerMeshes.emplace_back(BonesPerMesh);
		}

		// 鬆らせ繝・・繧ｿ縺ｫ繝懊・繝ｳ諠・ｱ繧偵そ繝・ヨ縺吶ｋ
		SetBoneDataToVertices();

		// 繝懊・繝ｳ縺ｮ繝弱・繝牙錐繝・Μ繝ｼ繧堤函謌舌☆繧・
		CreateNodeTree(pScene->mRootNode, &g_AssimpNodeNameTree);
	}

	// 繝・ぅ繝輔Η繝ｼ繧ｺ・｣・ｴ・・ｽ假ｽ費ｽ包ｽ抵ｽ・さ繝ｳ繝・リ繧定ｿ斐☆
	std::vector<std::unique_ptr<CTexture>> GetDiffuseTextures()
	{
		return std::move(g_diffuseTextures);
	}

	// 繝槭ユ繝ｪ繧｢繝ｫ諠・ｱ繧抵ｽ・ｽ難ｽ難ｽ会ｽ搾ｽ舌ｒ菴ｿ逕ｨ縺励※蜿門ｾ励☆繧・
	void GetMaterialData(const aiScene* pScene,std::string texturedirectory)
	{
		// 繝槭ユ繝ｪ繧｢繝ｫ謨ｰ蛻・ユ繧ｯ繧ｹ繝√Ε譬ｼ邏阪お繝ｪ繧｢繧堤畑諢上☆繧・
		g_diffuseTextures.resize(pScene->mNumMaterials);

		// 繝槭ユ繝ｪ繧｢繝ｫ謨ｰ譁・Ν繝ｼ繝・
		for (unsigned int m = 0; m < pScene->mNumMaterials; m++)
		{
			aiMaterial* material = pScene->mMaterials[m];

			// 繝槭ユ繝ｪ繧｢繝ｫ蜷榊叙蠕・
			std::string mtrlname = std::string(material->GetName().C_Str());
			std::cout << mtrlname << std::endl;

			// 繝槭ユ繝ｪ繧｢繝ｫ諠・ｱ
			aiColor4D ambient;
			aiColor4D diffuse;
			aiColor4D specular;
			aiColor4D emission;
			float shiness;

			// 繧｢繝ｳ繝薙お繝ｳ繝・
			if (AI_SUCCESS == aiGetMaterialColor(material, AI_MATKEY_COLOR_AMBIENT, &ambient)) {
			}
			else {
					ambient = aiColor4D(0.0f, 0.0f, 0.0f, 0.0f);
			}

			// 繝・ぅ繝輔Η繝ｼ繧ｺ
			aiColor3D baseColor{};
			if (AI_SUCCESS == material->Get(AI_MATKEY_BASE_COLOR, baseColor)) {
				 diffuse = aiColor4D(baseColor.r, baseColor.g, baseColor.b, 1.0f);
			}
			else if (AI_SUCCESS == aiGetMaterialColor(material, AI_MATKEY_COLOR_DIFFUSE, &diffuse)) {
			}
			else {
				 diffuse = aiColor4D(1.0f, 1.0f, 1.0f, 1.0f);
			}

			if (AI_SUCCESS == aiGetMaterialColor(material, AI_MATKEY_COLOR_SPECULAR, &specular)) {
			}
			else {
					specular = aiColor4D(0.0f, 0.0f, 0.0f, 0.0f);
			}

			// 繧ｨ繝溘ャ繧ｷ繝ｧ繝ｳ
			if (AI_SUCCESS == aiGetMaterialColor(material, AI_MATKEY_COLOR_EMISSIVE, &emission)) {
			}
			else {
					emission = aiColor4D(0.0f, 0.0f, 0.0f, 0.0f);
			}

			// 繧ｷ繝｣繧､繝阪せ
			if (AI_SUCCESS == aiGetMaterialFloat(material, AI_MATKEY_SHININESS, &shiness)) {
			}
			else {
					shiness = 0.0f;
			}

			// 縺薙・繝槭ユ繝ｪ繧｢繝ｫ縺ｫ邏舌▼縺・※縺・ｋ繝・ぅ繝輔Η繝ｼ繧ｺ繝・け繧ｹ繝√Ε謨ｰ蛻・Ν繝ｼ繝・
			std::vector<std::string> texpaths{};

			const aiTextureType diffuseTextureType =
				material->GetTextureCount(aiTextureType_DIFFUSE) > 0
				? aiTextureType_DIFFUSE
				: aiTextureType_BASE_COLOR;

			for (unsigned int t = 0; t < material->GetTextureCount(diffuseTextureType); t++)
			{
				aiString path{};

				if (AI_SUCCESS != material->Get(AI_MATKEY_TEXTURE(diffuseTextureType, t), path))
				{
					continue;
				}

				std::string texpath = std::string(path.C_Str());
				texpaths.push_back(texpath);

				if (auto tex = pScene->GetEmbeddedTexture(path.C_Str()))
				{
					auto texture = std::make_unique<CTexture>();
					bool sts = texture->LoadFromFemory(
						(unsigned char*)tex->pcData,
						tex->mWidth);
					if (sts)
					{
						g_diffuseTextures[m] = std::move(texture);
					}
				}
				else
				{
					auto texture = std::make_unique<CTexture>();
					std::string texname = texturedirectory + std::string(1, static_cast<char>(47)) + texpath;
					if (texture->Load(texname))
					{
						g_diffuseTextures[m] = std::move(texture);
					}
				}
			}

			
			MATERIAL mtrl{};

			mtrl.mtrlname = mtrlname;
			mtrl.Ambient = ambient;
			mtrl.Diffuse = diffuse;
			mtrl.Specular = specular;
			mtrl.Emission = emission;
			mtrl.Shiness = shiness;

			if (texpaths.size() == 0)
				mtrl.diffusetexturename = "";
			else
				mtrl.diffusetexturename = texpaths[0];

			g_materials.push_back(mtrl);
		}

	}

	void GetModelData(
		std::string filename,
		std::string texturedirectory,
		bool pretransformVertices)
	{
		// 譌･譛ｬ隱槭ヱ繧ｹ蟇ｾ遲厄ｼ喃ilesystem::path 竊・UTF-8 string・医≠縺ｪ縺溘・ utility 縺ｫ蜷医ｏ縺帙ｋ・・
		const std::string& srcUtf8 = filename;

		// 繧ｷ繝ｼ繝ｳ諠・ｱ讒狗ｯ・
		Assimp::Importer importer;

		// 繧ｷ繝ｼ繝ｳ諠・ｱ繧呈ｧ狗ｯ・
		unsigned int importFlags =
			aiProcessPreset_TargetRealtime_MaxQuality |
			aiProcess_ConvertToLeftHanded |
			aiProcess_PopulateArmatureData;
		if (pretransformVertices)
		{
			importFlags |= aiProcess_PreTransformVertices;
		}

		const aiScene* pScene = importer.ReadFile(
			srcUtf8.c_str(),
			importFlags);

		if (pScene == nullptr)
		{
			std::cout << "load error" << filename.c_str() << importer.GetErrorString() << std::endl;
		}
		assert(pScene != nullptr);

		// 隱ｭ縺ｿ霎ｼ縺ｿ鬆伜沺繧偵け繝ｪ繧｢
		g_vertices.clear();				//20240908
		g_indices.clear();				//20240908	
		g_materials.clear();			//20240908
		g_diffuseTextures.clear();		//20240908
		g_subsets.clear();				//20240908
		g_BoneDictionary.clear();		//20240908
		g_BonesPerMeshes.clear();		//20240908
		g_NodeLocalMatrices.clear();

		// 繝槭ユ繝ｪ繧｢繝ｫ諠・ｱ蜿門ｾ・
		GetMaterialData(pScene,texturedirectory);

		g_vertices.resize(pScene->mNumMeshes);

		for (unsigned int m = 0; m < pScene->mNumMeshes; m++)
		{
			aiMesh* mesh = pScene->mMeshes[m];
			g_vertices[m].reserve(mesh->mNumVertices);

			// 繝｡繝・す繝･蜷榊叙蠕・

			//縲鬆らせ謨ｰ蛻・Ν繝ｼ繝・
			for (unsigned int vidx = 0; vidx < mesh->mNumVertices; vidx++)
			{
				// 鬆らせ繝・・繧ｿ
				VERTEX	v{};

				// 蠎ｧ讓・	
				v.pos = mesh->mVertices[vidx];

				// 縺薙・鬆らせ縺御ｽｿ逕ｨ縺励※縺・ｋ繝槭ユ繝ｪ繧｢繝ｫ縺ｮ繧､繝ｳ繝・ャ繧ｯ繧ｹ逡ｪ蜿ｷ・医Γ繝・す繝･蜀・・・・
				// 繧剃ｽｿ逕ｨ縺励※繝槭ユ繝ｪ繧｢繝ｫ蜷阪ｒ繧ｻ繝・ヨ


				// 豕慕ｷ壹≠繧奇ｼ・
				if (mesh->HasNormals()) {
					v.normal = mesh->mNormals[vidx];
				}
				else
				{
					v.normal = aiVector3D(0.0f, 0.0f, 0.0f);
				}

				// 鬆らせ繧ｫ繝ｩ繝ｼ・滂ｼ茨ｼ千分逶ｮ・・
				if (mesh->HasVertexColors(0)) {
					v.color = mesh->mColors[0][vidx];
				}
				else
				{
					v.color = aiColor4D(1.0f, 1.0f, 1.0f, 1.0f);
				}

				// 繝・け繧ｹ繝√Ε縺ゅｊ・滂ｼ茨ｼ千分逶ｮ・・
				if (mesh->HasTextureCoords(0)) {
					v.texcoord = mesh->mTextureCoords[0][vidx];
				}
				else
				{
					v.texcoord = aiVector3D(0.0f, 0.0f, 0.0f);
				}

				// 鬆らせ繝・・繧ｿ繧定ｿｽ蜉
				g_vertices[m].push_back(v);
			}
		}

		// 繝｡繝・す繝･謨ｰ譁・Ν繝ｼ繝・
		// 繧､繝ｳ繝・ャ繧ｯ繧ｹ繝・・繧ｿ菴懈・
		g_indices.resize(pScene->mNumMeshes);
		for (unsigned int m = 0; m < pScene->mNumMeshes; m++)
		{
			aiMesh* mesh = pScene->mMeshes[m];
			g_indices[m].reserve(static_cast<size_t>(mesh->mNumFaces) * 3);

			// 繝｡繝・す繝･蜷榊叙蠕・

			// 繧､繝ｳ繝・ャ繧ｯ繧ｹ謨ｰ蛻・Ν繝ｼ繝・
			for (unsigned int fidx = 0; fidx < mesh->mNumFaces; fidx++)
			{
				aiFace face = mesh->mFaces[fidx];

//				assert(face.mNumIndices == 3);	// 荳芽ｧ貞ｽ｢縺ｮ縺ｿ蟇ｾ蠢・  car000.x縲蟇ｾ蠢・
 				assert(face.mNumIndices <= 3);	// 荳芽ｧ貞ｽ｢莉･荳九〒縺ゅｌ縺ｰOK・育ｸｮ騾繝昴Μ繧ｴ繝ｳ・・

				// 繧､繝ｳ繝・ャ繧ｯ繧ｹ繝・・繧ｿ繧定ｿｽ蜉
				for (unsigned int i = 0; i < face.mNumIndices; i++)
				{
					g_indices[m].push_back(face.mIndices[i]);
				}
			}
		}

		// 繧ｵ繝悶そ繝・ヨ諠・ｱ繧堤函謌・
		g_subsets.resize(pScene->mNumMeshes);
		for (unsigned int m = 0; m < g_subsets.size(); m++)
		{
			g_subsets[m].IndexNum = static_cast<unsigned int>(g_indices[m].size());
			g_subsets[m].VertexNum = static_cast<unsigned int>(g_vertices[m].size());
			g_subsets[m].VertexBase = 0;
			g_subsets[m].IndexBase = 0;
			g_subsets[m].meshname = std::string(pScene->mMeshes[m]->mName.C_Str());
			g_subsets[m].mtrlname = g_materials[pScene->mMeshes[m]->mMaterialIndex].mtrlname;
			g_subsets[m].materialindex = pScene->mMeshes[m]->mMaterialIndex;
		}

		// 繧ｵ繝悶そ繝・ヨ諠・ｱ繧堤嶌蟇ｾ逧・↑繧ゅ・縺ｫ縺吶ｋ	
		for (int m = 0; m < g_subsets.size(); m++)
		{
			// 鬆らせ繝舌ャ繝輔ぃ縺ｮ繝吶・繧ｹ繧定ｨ育ｮ・
			g_subsets[m].VertexBase = 0;
			for (int i = m - 1; i >= 0; i--) {
				g_subsets[m].VertexBase += g_subsets[i].VertexNum;
			}

			// 繧､繝ｳ繝・ャ繧ｯ繧ｹ繝舌ャ繝輔ぃ縺ｮ繝吶・繧ｹ繧定ｨ育ｮ・
			g_subsets[m].IndexBase = 0;
			for (int i = m - 1; i >= 0; i--) {
				g_subsets[m].IndexBase += g_subsets[i].IndexNum;
			}
		}

		// 繝懊・繝ｳ諠・ｱ蜿門ｾ・
		GetBone(pScene);
	}

	std::vector<SUBSET> GetSubsets() 
	{
		return std::move(g_subsets);
	}

		// 繧ｵ繝悶そ繝・ヨ諠・ｱ{
	std::vector<std::vector<VERTEX>> GetVertices() 
	{
		return std::move(g_vertices);		// 鬆らせ繝・・繧ｿ・医Γ繝・す繝･蜊倅ｽ搾ｼ・
	}

	std::vector<std::vector<unsigned int>> GetIndices() 
	{
		return std::move(g_indices);		// 繧､繝ｳ繝・ャ繧ｯ繧ｹ繝・・繧ｿ・医Γ繝・す繝･蜊倅ｽ搾ｼ・
	}

	std::vector<MATERIAL> GetMaterials()
	{
		return g_materials;		// 繝槭ユ繝ｪ繧｢繝ｫ
	}
}
}
}
