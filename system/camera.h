#pragma once

#include	"commontypes.h"
#include	"renderer.h"
#include	"CPolar3D.h"
#include	"../application.h"

class Camera {
protected:
	Vector3	m_position = Vector3(0.0f, 50.0f, -100.0f);	// カメラ位置

	Vector3		m_lookat{0,0,0};				// 注視点
	Vector3		m_up = { 0,1,0 };		// アップベクトル			
	Matrix4x4	m_viewmtx{};			// ビュー変換行列
	Matrix4x4   m_projmtx{};			// プロジェクション行列

public:
	virtual ~Camera(){}
	Camera() = default;

	Camera(Vector3 pos, Vector3 lookat,Vector3 up)
		:m_position(pos),m_lookat(lookat),m_up(up) {}

	void Init();
	void Dispose();
	void Update();
	void Draw();
	void SetPosition(const Vector3& position) { m_position = position; }
	void SetLookat(const Vector3& position) { m_lookat = position; }
	void SetUP(const Vector3& up) { m_up = up; }

	void Draw2D() {
		Renderer::SetWorldViewProjection2D();
	}


	Matrix4x4 GetViewMatrix() const { return m_viewmtx; }
	Matrix4x4 GetProjMatrix() const { return m_projmtx; }

	Vector3 GetPosition() const{
		return m_position;
	}
	Vector3 GetLookat() const{
		return m_lookat;
	}
	Vector3 GetUP() const {
		return m_up;
	}
};