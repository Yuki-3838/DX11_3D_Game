#pragma once
#include <string>
#include <memory>
#include <unordered_map>
#include <functional>
#include <cassert>
#include "IScene.h"

/**
 * @brief シーンのクラスを名前で登録・生成するためのファクトリクラス
 *
 * @details このクラスはシングルトンとして動作し、文字列名に対応する IScene 派生クラスの
 * インスタンスを生成するための関数を登録・管理します。create() 関数を通じて文字列から
 * シーンインスタンスを動的に生成できます。
 */
class SceneClassFactory {
public:
    /**
     * @brief IScene 派生クラスのインスタンスを生成する関数型
     */
    using SceneCreatorFunc = std::function<std::unique_ptr<IScene>()>;

    /**
     * @brief シングルトンインスタンスを取得
     *
     * @return SceneClassFactory の唯一のインスタンス
     */
    static SceneClassFactory& getInstance() {
        static SceneClassFactory instance;
        return instance;
    }

    /**
     * @brief クラス名と生成関数を登録する
     *
     * @param name クラス名（create() で指定するキー）
     * @param func クラスインスタンスを生成する関数（例：std::make_unique）
     */
    void registerClass(const std::string& name, SceneCreatorFunc func) {
        registry[name] = func;
    }

    /**
     * @brief 登録されたクラス名からシーンインスタンスを生成する
     *
     * @param name 生成したいクラスの名前（registerClass で登録されたキー）
     * @return std::unique_ptr<IScene> 該当クラスのユニークポインタ（見つからなければ nullptr）
     */
    std::unique_ptr<IScene> create(const std::string& name) {
        auto it = registry.find(name);
        if (it != registry.end()) {
            return it->second();
        }
        assert(false && "SceneClassFactory: scene class is not registered");
        return nullptr;
    }

private:
    /**
     * @brief クラス名と生成関数のマッピングテーブル
     */
    std::unordered_map<std::string, SceneCreatorFunc> registry;
};

/**
 * @brief クラスを SceneClassFactory に自動登録するマクロ
 *
 * @details IScene 派生クラスのソースファイルにこのマクロを記述することで、静的に
 * SceneClassFactory に登録され、文字列による動的生成が可能になります。
 *
 * 使用例：
 * @code
 * class TitleScene : public IScene { ... };
 * REGISTER_CLASS(TitleScene);
 * @endcode
 *
 * @param CLASSNAME 登録対象のクラス名
 */
#define REGISTER_CLASS(CLASSNAME) \
    namespace { \
        struct CLASSNAME##Registrar { \
            CLASSNAME##Registrar() { \
                SceneClassFactory::getInstance().registerClass(#CLASSNAME, []() { \
                    return std::make_unique<CLASSNAME>(); \
                }); \
            } \
        }; \
        static CLASSNAME##Registrar global_##CLASSNAME##_registrar; \
    }
