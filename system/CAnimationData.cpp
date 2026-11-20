#include	<cassert>
#include	<iostream>
#include	"CAnimationData.h"
#include	"ModelCache.h"

namespace
{
	std::vector<uint32_t> ClipCacheLayout()
	{
		return {
			static_cast<uint32_t>(sizeof(ai_real)),
			static_cast<uint32_t>(sizeof(double)) };
	}

	void WriteVector(ModelCache::Writer& writer, const aiVector3D& value)
	{
		writer.Pod(value.x);
		writer.Pod(value.y);
		writer.Pod(value.z);
	}

	aiVector3D ReadVector(ModelCache::Reader& reader)
	{
		aiVector3D value;
		value.x = reader.Pod<ai_real>();
		value.y = reader.Pod<ai_real>();
		value.z = reader.Pod<ai_real>();
		return value;
	}

	/** ファイルに入っていたアニメーションを書く(Assimp で読んだ直後・保存ファイルの両方で同じ形)。 */
	void WriteAnimations(ModelCache::Writer& writer, const aiScene& scene)
	{
		writer.U32(scene.mNumAnimations);
		for (unsigned int a = 0; a < scene.mNumAnimations; ++a)
		{
			const aiAnimation& animation = *scene.mAnimations[a];
			writer.String(animation.mName.C_Str());
			writer.Pod(animation.mDuration);
			writer.Pod(animation.mTicksPerSecond);
			writer.U32(animation.mNumChannels);
			for (unsigned int c = 0; c < animation.mNumChannels; ++c)
			{
				const aiNodeAnim& channel = *animation.mChannels[c];
				writer.String(channel.mNodeName.C_Str());
				writer.U32(static_cast<uint32_t>(channel.mPreState));
				writer.U32(static_cast<uint32_t>(channel.mPostState));
				writer.U32(channel.mNumPositionKeys);
				for (unsigned int k = 0; k < channel.mNumPositionKeys; ++k)
				{
					writer.Pod(channel.mPositionKeys[k].mTime);
					WriteVector(writer, channel.mPositionKeys[k].mValue);
				}
				writer.U32(channel.mNumRotationKeys);
				for (unsigned int k = 0; k < channel.mNumRotationKeys; ++k)
				{
					const aiQuaternion& q = channel.mRotationKeys[k].mValue;
					writer.Pod(channel.mRotationKeys[k].mTime);
					writer.Pod(q.w);
					writer.Pod(q.x);
					writer.Pod(q.y);
					writer.Pod(q.z);
				}
				writer.U32(channel.mNumScalingKeys);
				for (unsigned int k = 0; k < channel.mNumScalingKeys; ++k)
				{
					writer.Pod(channel.mScalingKeys[k].mTime);
					WriteVector(writer, channel.mScalingKeys[k].mValue);
				}
			}
		}
	}

	/** WriteAnimations で書いたものから aiAnimation を作り直す(メッシュのアニメーションは使っていないので持たない)。 */
	bool ReadAnimations(ModelCache::Reader& reader, std::vector<std::unique_ptr<aiAnimation>>& out)
	{
		const uint32_t animationCount = reader.U32();
		for (uint32_t a = 0; a < animationCount && reader.Ok(); ++a)
		{
			auto animation = std::make_unique<aiAnimation>();
			animation->mName.Set(reader.String());
			animation->mDuration = reader.Pod<double>();
			animation->mTicksPerSecond = reader.Pod<double>();
			const uint32_t channelCount = reader.U32();
			if (!reader.Ok() || channelCount > 100000)
				return false;
			if (channelCount > 0)
			{
				animation->mChannels = new aiNodeAnim * [channelCount] {};
				animation->mNumChannels = channelCount;
			}
			for (uint32_t c = 0; c < channelCount && reader.Ok(); ++c)
			{
				aiNodeAnim* channel = new aiNodeAnim();
				animation->mChannels[c] = channel;
				channel->mNodeName.Set(reader.String());
				channel->mPreState = static_cast<aiAnimBehaviour>(reader.U32());
				channel->mPostState = static_cast<aiAnimBehaviour>(reader.U32());

				const uint32_t positionCount = reader.U32();
				if (!reader.Ok() || positionCount > 10000000)
					return false;
				channel->mPositionKeys = positionCount > 0 ? new aiVectorKey[positionCount] : nullptr;
				channel->mNumPositionKeys = positionCount;
				for (uint32_t k = 0; k < positionCount; ++k)
				{
					channel->mPositionKeys[k].mTime = reader.Pod<double>();
					channel->mPositionKeys[k].mValue = ReadVector(reader);
				}

				const uint32_t rotationCount = reader.U32();
				if (!reader.Ok() || rotationCount > 10000000)
					return false;
				channel->mRotationKeys = rotationCount > 0 ? new aiQuatKey[rotationCount] : nullptr;
				channel->mNumRotationKeys = rotationCount;
				for (uint32_t k = 0; k < rotationCount; ++k)
				{
					channel->mRotationKeys[k].mTime = reader.Pod<double>();
					aiQuaternion& q = channel->mRotationKeys[k].mValue;
					q.w = reader.Pod<ai_real>();
					q.x = reader.Pod<ai_real>();
					q.y = reader.Pod<ai_real>();
					q.z = reader.Pod<ai_real>();
				}

				const uint32_t scalingCount = reader.U32();
				if (!reader.Ok() || scalingCount > 10000000)
					return false;
				channel->mScalingKeys = scalingCount > 0 ? new aiVectorKey[scalingCount] : nullptr;
				channel->mNumScalingKeys = scalingCount;
				for (uint32_t k = 0; k < scalingCount; ++k)
				{
					channel->mScalingKeys[k].mTime = reader.Pod<double>();
					channel->mScalingKeys[k].mValue = ReadVector(reader);
				}
			}
			out.push_back(std::move(animation));
		}
		return reader.Ok();
	}
}

bool CAnimationData::LoadAnimation(const std::string& filename, const std::string& name)
{
	std::vector<std::unique_ptr<aiAnimation>> animations;

	// 保存ファイル(ModelCache.h)が使えれば、そこから読む。
	ModelCache::Header header;
	const bool useCache = ModelCache::Enabled() &&
		ModelCache::MakeHeader("clip", filename, std::string(), ClipCacheLayout(), header);
	const std::filesystem::path cachePath = useCache
		? ModelCache::PathFor(header.kind, filename, std::string())
		: std::filesystem::path();
	bool loaded = false;
	if (useCache)
	{
		ModelCache::Reader reader;
		loaded = reader.Open(cachePath) && ModelCache::ReadAndCheckHeader(reader, header) &&
			ReadAnimations(reader, animations) && reader.AtEnd();
		if (!loaded)
			animations.clear();
	}

	if (!loaded)
	{
		Assimp::Importer importer;
		const aiScene* scene = importer.ReadFile(filename.c_str(), aiProcess_ConvertToLeftHanded);
		if (scene == nullptr)
		{
			std::cout << " animation load error " << filename << " " << importer.GetErrorString() << std::endl;
			m_Animation[name].clear();
			assert(scene != nullptr);
			return false;
		}
		// アニメーションだけを書き出し、それを読み戻して持つ(Assimp のシーンは、ここで捨てる)。
		// 保存ファイルを使わないときも同じ道を通るので、保存ファイルの読み書きが合っていることも毎回確かめられる。
		ModelCache::Writer writer;
		if (useCache)
			ModelCache::WriteHeader(writer, header);
		const size_t bodyStart = writer.Size();
		WriteAnimations(writer, *scene);
		ModelCache::Reader reader;
		reader.Assign(writer.Data() + bodyStart, writer.Size() - bodyStart);
		if (!ReadAnimations(reader, animations) || !reader.AtEnd())
		{
			std::cout << " animation copy error " << filename << std::endl;
			animations.clear();
		}
		else if (useCache && writer.SaveTo(cachePath))
		{
			std::cout << "[ModelCache] saved " << cachePath.string() << std::endl;
		}
	}

	std::vector<aiAnimation*>& entry = m_Animation[name];
	entry.clear();
	for (auto& animation : animations)
	{
		entry.push_back(animation.get());
		m_ownedAnimations.push_back(std::move(animation));
	}
	return !entry.empty();
}

// 指定した名前のアニメーションデータを取得する
aiAnimation* CAnimationData::GetAnimation(const char* name, int idx) {
	const auto it = m_Animation.find(name);
	if (it == m_Animation.end() || idx < 0 || static_cast<size_t>(idx) >= it->second.size())
	{
		std::cout << " animation not found " << name << " index " << idx << std::endl;
		return nullptr;
	}

	return it->second[idx];
}
