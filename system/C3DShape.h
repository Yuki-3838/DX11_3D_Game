#pragma once
#include <string>
#include "CommonTypes.h"
#include "transform.h"

/**
 * @brief 3D図形の抽象基底クラス
 *
 * すべての3D図形クラスはこのクラスを継承し、描画やサイズ設定のための
 * インターフェースを実装する必要があります。
 */
class C3DShape {
protected:
	static bool m_first;                // <-- 初期化フラグ
public:
    /**
     * @brief コンストラクタ
     */
    C3DShape();

    /**
     * @brief デストラクタ
     */
    virtual ~C3DShape() {}

    /**
     * @brief SRT（Scale, Rotation, Translation）で図形を描画
     * @param srt スケール・回転・平行移動情報
     * @param col 描画色
     */
    virtual void Draw(SRT srt, Color col) = 0;

    /**
     * @brief 行列を使用して図形を描画
     * @param mtx 描画に使用する変換行列
     * @param col 描画色
     */
    virtual void Draw(Matrix4x4 mtx, Color col) = 0;

    /**
     * @brief 図形のサイズを設定
     * @param radius 半径または幅
     * @param height 高さ
     * @param depth 奥行き
     */
    virtual void SetSize(float radius, float height, float depth) = 0;

    /**
     * @brief 図形の名前を取得
     * @return 図形の名前文字列
     */
    virtual std::string getName() const = 0;
};

/**
 * @brief 球体の3D図形クラス
 */
class Sphere : public C3DShape {
private:
    float m_radius;
public:
    /**
        * @brief コンストラクタ
        * @param r 半径
    */
    Sphere(float r) : m_radius(r) {}

    void Draw(SRT srt, Color col) override;
    void Draw(Matrix4x4 mtx, Color col) override;

    /**
     * @brief 半径を設定
    * @param r 半径
    */
	void SetRadius(float r) {
		m_radius = r;
	}

    void SetSize(float radius, float height=0 ,float depth=0) override {
		m_radius = radius;
    }

    std::string getName() const override {
        return "Sphere";
    }
};

/**
 * @brief 円錐の3D図形クラス
 */
class Cone : public C3DShape {
private:
    float m_radius; ///< 底面の半径
    float m_height; ///< 高さ
public:
    Cone(float r, float h) : m_radius(r), m_height(h) {}

    void Draw(SRT srt, Color col) override;

    void Draw(Matrix4x4 mtx, Color col);

    void SetSize(float radius, float height, float depth) override {
        m_height = height;
        m_radius = radius;
    }

    std::string getName() const override {
        return "Cone";
    }
};
/**
 * @brief ボックス（直方体）の3D図形クラス
 */
class Box : public C3DShape {
private:
    float m_width;
    float m_height;
    float m_depth;
public:
    Box(float w, float h, float d) : m_width(w), m_height(h), m_depth(d) {}

    void Draw(SRT srt, Color col) override;

    void Draw(Matrix4x4 mtx, Color col);

    void SetSize(float width, float height, float depth) override {
        m_width = width;
		m_height = height;
		m_depth = depth;
	}

    std::string getName() const override {
        return "Box";
    }
};

/**
 * @brief 円柱の3D図形クラス
 */
class Cylinder : public C3DShape {
private:
    float m_radius;
    float m_height;
public:
    Cylinder(float r, float h) : m_radius(r), m_height(h) {}

    void Draw(SRT srt, Color col) override;

    void Draw(Matrix4x4 mtx, Color col) override;

    void SetSize(float radius, float height,float depth =0 ) override {
        m_height = height;
        m_radius = radius;
    }

	float GetRadius() const {
		return m_radius;
	}

    float GetHeight() const {
        return m_height;
    }

    std::string getName() const override {
        return "Cylinder";
    }
};

/**
 * @brief 線分（セグメント）の3D図形クラス
 */
class Segment : public C3DShape {
private:
    Vector3 m_start;     ///< 始点
    Vector3 m_end;       ///< 終点
    Vector3 m_direction; ///< 方向ベクトル
    float m_length;      ///< 長さ
    float m_width{2};       ///< 線の太さ  
public:
    Segment(Vector3 s,Vector3 e):m_start(s),m_end(e),m_direction(e-s),m_length((e-s).Length()){}

    void SetWidth(float w) {
		m_width = w;
    }

    void Draw(SRT srt, Color col) override;

    void Draw(Matrix4x4 mtx, Color col) override;

    void SetSize(float radius, float height, float depth) override {
    }

    std::string getName() const override {
        return "Segment";
    }
};

/**
 * @brief カプセル（円柱＋球）の3D図形クラス
 */
class Capsule : public C3DShape {
private:
    float m_radius; ///< 半径
    float m_height; ///< 高さ
public:
    Capsule(float r, float h) : m_radius(r), m_height(h) {}

    void SetRadius(float r) {
        m_radius = r;
    }

    void SetSize(float radius, float height ,float depth=0) override{
        m_height = height;
        m_radius = radius;
    }
    
    float GetHeight() const {
        return m_height;
    }

    float GetRadius() const {
        return m_radius;
    }
    void Draw(SRT srt, Color col) override;

    void Draw(Matrix4x4 mtx, Color col) override;

    std::string getName() const override {
        return "Capsule";
    }
};