#pragma once
#include "commontypes.h"
#include "utility.h"
#include "ConeDrawer.h"
#include "SphereDrawer.h"

class AimOrientation {

	const Vector3 baseorien={0,1,0};
	Quaternion m_quaternion{};
	Vector3 m_targetvector{};

public:

	AimOrientation() = delete;
	~AimOrientation() {}

	AimOrientation(Vector3 targetvector) {
		m_quaternion = utility::CreateTargetQuaternion(
			baseorien,
			targetvector);
		m_targetvector = targetvector;
	}

	Matrix4x4 ToMatrix() {
		return Matrix4x4::CreateFromQuaternion(m_quaternion);
	}
	
	void VisualizeDirection(
		Vector3 centerposition,
		float coneheight,
		float coneradius,
		Color conecolor,
		float sphereradius,
		Color spherecolor) 
	{

		Matrix4x4 mtx = ToMatrix();
		ConeDrawerDraw(coneradius, coneheight, conecolor, 
			centerposition.x, centerposition.y, centerposition.z, 
			mtx);

		Vector3 pos = (m_targetvector* coneheight) + centerposition;
		SphereDrawerDraw(sphereradius, spherecolor, pos.x, pos.y, pos.z);

		SphereDrawerDraw(coneheight, Color(1,1,1,0.3f), centerposition.x, centerposition.y, centerposition.z);
	}
};
