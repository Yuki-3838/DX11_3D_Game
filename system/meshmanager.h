#pragma once
#include <memory>
#include <unordered_map>
#include <string>
#include "CMesh.h"
#include "CMeshRenderer.h"
#include "CShader.h"
#include "noncopyable.h"

// 共通のリソース管理用テンプレートクラス
template <typename ResourceType>
class ResourceManager : NonCopyable {
    static inline std::unordered_map<std::string, std::unique_ptr<ResourceType>> m_container{};
public:
    template<class T>
    static bool Register(const std::string& key, std::unique_ptr<T> data) {
        // 存在してる場合は何もしない
        if (m_container.contains(key)) return false;

        // 存在していなければムーブして登録 (emplaceを使用すると効率的です)
        m_container.emplace(key, std::move(data));
        return true;
    }

    template<class T = ResourceType>
    static T* Get(const std::string& key) {
        // [重要] operator[] はキーが存在しない場合に空の要素を新規作成してしまうため find を使用する
        auto it = m_container.find(key);
        if (it != m_container.end()) {
            return static_cast<T*>(it->second.get());
        }
        return nullptr; // 見つからない場合は nullptr を返す
    }

    // コンテナに指定されたキーのリソースが存在するかチェックする
    static bool Contains(const std::string& key) {
        return m_container.contains(key);
    }
};

using ShaderManager = ResourceManager<CShader>;

class MeshManager : NonCopyable {
public:
    // --- Mesh ---
    template<class T>
    static bool RegisterMesh(const std::string& key, std::unique_ptr<T> meshdata) {
        return ResourceManager<CMesh>::Register(key, std::move(meshdata));
    }

    template<class T = CMesh>
    static T* getMesh(const std::string& key) {
        return ResourceManager<CMesh>::Get<T>(key);
    }

    // ▼ 追加: Meshの存在チェック
    static bool ContainsMesh(const std::string& key) {
        return ResourceManager<CMesh>::Contains(key);
    }

    // --- MeshRenderer ---
    template<class T>
    static bool RegisterMeshRenderer(const std::string& key, std::unique_ptr<T> data) {
        return ResourceManager<CMeshRenderer>::Register(key, std::move(data));
    }

    template<class T = CMeshRenderer>
    static T* getRenderer(const std::string& key) {
        return ResourceManager<CMeshRenderer>::Get<T>(key);
    }

    // ▼ 追加: MeshRendererの存在チェック
    static bool ContainsRenderer(const std::string& key) {
        return ResourceManager<CMeshRenderer>::Contains(key);
    }
};