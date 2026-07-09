#pragma once
#include	"CommonTypes.h"
#include	"transform.h"
#include	"C3DShape.h"
#include	"cplane.h"
#include	<algorithm>

namespace GM31 {namespace GE {namespace {}
	namespace Collision {
		// 円柱定義
		struct BoundingCylinder {
			Vector3    bottom;						// 底
			Vector3    top;							// 上
			float      radius;						// 半径
		};

		// 球定義
		struct BoundingSphere {
			Vector3	center;
			float	radius;
		};

		// セグメント（線分）
		struct Segment {
			Vector3		startpoint;				// 開始点
			Vector3		endpoint;				// 終了点
		};

		// 直線
		struct Line {
			Vector3		point;					// 直線上の任意の点
			Vector3		direction;				// 方向
		};

		// 境界ボックスAABB
		struct BoundingBoxAABB {
			Vector3 min;
			Vector3 max;
		};

		// 境界ボックスOBB
		struct BoundingBoxOBB : public BoundingBoxAABB {
			Vector3 center;
			Vector3 worldcenter;

			// 中心軸（X軸、Y軸、Z軸）
			Vector3 axisX;
			Vector3 axisY;
			Vector3 axisZ;

			// BOXサイズ
			float	lengthx;
			float	lengthy;
			float	lengthz;
		};

		// 境界カプセル
		struct BoundingCapsule {
			Vector3		startpoint;				// 開始点
			Vector3		endpoint;				// 終了点
			float		radius;					// 半径
		};

		// 球
		bool CollisionSphere(BoundingSphere p1, BoundingSphere p2);

		// AABB
		bool CollisionAABB(BoundingBoxAABB p1, BoundingBoxAABB p2);

		// make AABB
		BoundingBoxAABB SetAABB(Vector3 centerposition, float width, float height, float depth);

		// 直線と点の距離を求める
		float calcPointLineDist(
			const Vector3& point,
			const Segment& segment,
			Vector3& intersectionpoint,
			float& t);

		// 直線と点の距離を求める
		float calcPointLineDist(
			const Vector3& point,
			const Segment& segment,
			Vector3& intersectionpoint,
			float& t);

		// 線分と直線の長さを求める
		float calcPointSegmentDist(
			const Vector3& p,
			const Segment& segment,
			Vector3& intersectionpoint,
			float& t);

		// 円柱と球の当たり判定
		bool CollisionSphereCylinder(
			BoundingSphere sphere,
			BoundingCylinder cylinder);

		// AABBと点の最近接点を求める
		void ClosestPtPointAABB(Vector3 p, BoundingBoxAABB aabb, Vector3& q);

		// AABBと点の距離を求める
		float SqDistPointAABB(Vector3 p, BoundingBoxAABB aabb);

		// AABBと球の当たり判定
		bool CollisionSphereAABB(
			BoundingSphere sphere,
			BoundingBoxAABB aabb);

		// OBBと点のもっと近い点を求める
		void ClosestPtPointOBB(Vector3 point, BoundingBoxOBB obb, Vector3& answer);

		// OBBと点の距離を求める
		float DistPointOBB(Vector3 point, BoundingBoxOBB obb);

		BoundingBoxOBB SetOBB(
			Vector3 rot,					// 姿勢（回転角度）
			Vector3 pos,					// 中心座標（ワールド）
			float width,					// 幅
			float height,					// 高さ
			float depth);					// 奥行き

		// OBBと球の当たり判定
		bool CollisionSphereOBB(
			BoundingSphere sphere,
			BoundingBoxOBB obb);

		// カプセル同士の当たり判定
		bool CollisionCapsule(BoundingCapsule p1, BoundingCapsule p2);

		// 線分と線分の最短距離を求める
		float ClosestPtSegmentSegment(
			const Segment& seg1,				// 線分１
			const Segment& seg2, 				// 線分２	
			float& s,							// 線分１の最短距離のパラメータ
			float& t,							// 線分２の最短距離のパラメータ
			Vector3& p1, 						// 線分１の最短距離の点	
			Vector3& p2);						// 線分２の最短距離の点

		// 全頂点を座標変換した後をAABB求める
		BoundingBoxAABB calcAABB(
			const std::vector<Vector3>& vertices,
			SRT transform);

		// AABBから8頂点を取得
		std::vector<Vector3> GetCorners(const BoundingBoxAABB& box);

		// 座標変換した後のAABBを求める
		BoundingBoxAABB TransformAABB(
			const BoundingBoxAABB& inAABB,
			SRT transform);

		// OBBとOBBの当たり判定
		bool CollisionOBB(
			const BoundingBoxOBB& obbA,
			const BoundingBoxOBB& obbB);

		// BOUNDING CYLINDERを作成する
		BoundingCylinder SetBoundingCylinder(
			SRT transform,		// 円柱の姿勢情報
			Cylinder cylinder);	// 描画用の円柱情報

		// 円柱同士の当たり判定	
		bool CollisionCylinder(
			const BoundingCylinder& Cylinder1,
			const BoundingCylinder& Cylinder2);

		// 線分上に指定した点が存在するかどうかをチェックする
		bool isPointOnSegment(
			const Segment& seg,
			const Vector3& point,
			float epsilon = 1e-6f);

		// 直線同士の最短距離を求める
		float ClosestDistanceBetweenLines(
			const Line& line1,
			const Line& line2,
			float& s, 			// line1 パラメータ（無制限）
			float& t,			// line2 パラメータ（無制限）
			Vector3& p1, 		// line1 最近接点
			Vector3& p2);		// line2 最近接点

		// 円柱同士の当たり判定精密版
		bool PreciseCylinderCollision(
			const BoundingCylinder& Cylinder1,
			const BoundingCylinder& Cylinder2);

		// 円柱同士の当たり判定(側面)
		bool CylinderSideIntersect(
			const BoundingCylinder& cylinderSide,
			const BoundingCylinder& other);

		// バウンディングカプセルを作成する
		BoundingCapsule SetBoundingCapsule(
			SRT transform,		// カプセルの姿勢情報
			float radius,		// カプセルの半径
			float height);		// カプセルの高さ

		// バウンディングスフィアを作成する add date 2025/06/28 by suzuki)
		BoundingSphere calcBSphere(
			const std::vector<Vector3>& vertices,
			SRT transform);

		// 直線と平面の交差判定
		bool LinetoPlaneCross(
			const Plane& plane,		//平面の方程式
			const Vector3& p0,		//直線の起点
			const Vector3& wv,		//直線の方向ベクトル
			float& t,				// 交点位置情報	
			Vector3& ans);			//交点座標

		// 三角形の内部かどうかを判定する
		bool CheckInTriangle(
			const Vector3& a,
			const Vector3& b,
			const Vector3& c,
			const Vector3& p);

	} // namespace Collision
}}// namespace GM31::GE::Collision