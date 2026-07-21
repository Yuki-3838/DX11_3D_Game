#pragma once
#include <memory>
#include <array>
#include <cstdint>
#include <string>
#include "../system/commontypes.h"
#include "../system/SceneClassFactory.h"
#include "../system/IScene.h"
#include "../system/C3DShape.h"
#include "../system/Camera.h"
#include "../system/collision.h"
#include "../system/CStaticMesh.h"
#include "../system/CStaticMeshRenderer.h"

class CarScene : public IScene 
{
	// 回転角度
	Vector3 m_Rotation{};

	// 現在の姿勢を表すクォータニオン
	Quaternion m_RotationQ{};

	// 現在の姿勢を表す行列
	Matrix4x4 m_RotationMtx{};

	Matrix4x4 m_ScaleMtx{};

	// 選択モデルのローカルAABB。モデル変更時に頂点から作り直す。
	GM31::GE::Collision::BoundingBoxAABB m_modelLocalAABB{};
	bool m_hasModelLocalAABB{ false };

	// 当たり判定の相手側モデル。こちらも頂点からAABB/OBBを作る。
	std::string m_targetMeshid{};
	GM31::GE::Collision::BoundingBoxAABB m_targetLocalAABB{};
	bool m_hasTargetLocalAABB{ false };
	Vector3 m_targetPosition{ 120.0f, 0.0f, 0.0f };
	Vector3 m_targetRotation{};
	Vector3 m_targetScale{ 1.0f, 1.0f, 1.0f };
public:
	explicit CarScene();
	void update(uint64_t deltatime) override;
	void draw(uint64_t deltatime) override;
	void init() override;
	void dispose() override;
	void debugRubikCubeRotation();
	void debugRubikCubeLocalRotation();

	void debugModelSelect();
	void debugBoundingBox();

private:
	Camera m_camera;									// 固定カメラ
	std::unique_ptr<Box> m_shapecube;					// 立方体
	std::array<std::unique_ptr<Segment>,3> m_segments;	// ローカル軸表示用線分

	// 今表示しているメッシュのID
	std::string m_meshid{};

	void loadModel(int modelIndex, std::string& meshid);
	void updateModelLocalAABB();
	void updateLocalAABB(
		const std::string& meshid,
		GM31::GE::Collision::BoundingBoxAABB& localAABB,
		bool& hasLocalAABB);
	Matrix4x4 calcTargetWorldMatrix() const;
	GM31::GE::Collision::BoundingBoxAABB calcWorldAABB(
		const GM31::GE::Collision::BoundingBoxAABB& localAABB,
		Matrix4x4 worldmtx) const;
	GM31::GE::Collision::BoundingBoxOBB calcModelOBB(
		const GM31::GE::Collision::BoundingBoxAABB& localAABB,
		Matrix4x4 worldmtx) const;
	void drawAABB(const GM31::GE::Collision::BoundingBoxAABB& aabb, Color col);
	void drawOBB(const GM31::GE::Collision::BoundingBoxOBB& obb, Color col);

};

REGISTER_CLASS(CarScene)
