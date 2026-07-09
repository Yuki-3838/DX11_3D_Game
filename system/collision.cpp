#include    "commontypes.h"
#include	"collision.h"

namespace GM31 {
	namespace GE {
		namespace {}
		namespace Collision {

			// OBBの重なりを判定する
			bool CompareLengthOBB(
				const BoundingBoxOBB& ObbA,		// OBB-A
				const BoundingBoxOBB& ObbB,		// OBB-B
				const Vector3& vecSeparate,		// 分離軸
				const Vector3& vecDistance)		// 中心座標を結んだベクトル
			{
				// 課題NO 03 [OBB当たり判定]で完成させてください



				return true;
			}

			// 線分に平行な与えられた幅の４角形の座標を求める
			void CalcQuadOrientedLine(
				Vector3 startpos,
				Vector3 endpos,
				Vector3* v,
				float width)
			{

				// 垂直なベクトルを求める	
				Vector3 zaxis(0, 0, 1);
				Vector3 line = endpos - startpos;

				Vector3 ans;
				ans = line.Cross(zaxis);					// 外積
				ans.Normalize();							// 正規化

				Vector3 startposplus = startpos + ans * width / 2.0f;
				Vector3 startposminus = startpos - ans * width / 2.0f;

				Vector3 endposplus = endpos + ans * width / 2.0f;
				Vector3 endposminus = endpos - ans * width / 2.0f;

				v[0] = startposplus;
				v[1] = endposplus;

				v[2] = startposminus;
				v[3] = endposminus;

			}

			// 線分に平行な与えられた幅の４角形の座標を求める
			void CalcQuadOrientedLine(
				Vector3 startpos,
				Vector3 endpos,
				Vector3 up,
				Vector3* v,
				float width)
			{

				// 垂直なベクトルを求める	
				Vector3 zaxis = up;
				Vector3 line = endpos - startpos;



				Vector3 ans;
				ans = line.Cross(zaxis);					// 外積
				ans.Normalize();							// 正規化

				Vector3 startposplus = startpos + ans * width / 2.0f;
				Vector3 startposminus = startpos - ans * width / 2.0f;

				Vector3 endposplus = endpos + ans * width / 2.0f;
				Vector3 endposminus = endpos - ans * width / 2.0f;

				v[0] = startposplus;
				v[1] = endposplus;

				v[2] = startposminus;
				v[3] = endposminus;

			}



			// 直線と点の距離を求める
			float calcPointLineDist(
				const Vector3& point,
				const Segment& segment,
				Vector3& intersectionpoint,
				float& t) {

				float distance = 0.0f;

				// 線分のベクトルを求める
				double ABx = segment.endpoint.x - segment.startpoint.x;
				double ABy = segment.endpoint.y - segment.startpoint.y;
				double ABz = segment.endpoint.z - segment.startpoint.z;

				// 線分の始点と点を結ぶベクトルを求める
				double APx = point.x - segment.startpoint.x;
				double APy = point.y - segment.startpoint.y;
				double APz = point.z - segment.startpoint.z;

				// 線分ABの長さを求める
				double AB2 = ABx * ABx + ABy * ABy + ABz * ABz;

				// ABベクトルとAPベクトルの内積を計算する（射影した長さを求める）
				double ABdotAP = ABx * APx + ABy * APy + ABz * APz;

				// ttを計算
				double tt = ABdotAP / AB2;

				// 垂線の足
				intersectionpoint.x = static_cast<float>(segment.startpoint.x + ABx * tt);
				intersectionpoint.y = static_cast<float>(segment.startpoint.y + ABy * tt);
				intersectionpoint.z = static_cast<float>(segment.startpoint.z + ABz * tt);

				t = static_cast<float>(tt);
				// 垂線の足の長さ
				distance = (intersectionpoint - point).Length();
				return distance;
			}

			// 線分と直線の長さを求める
			float calcPointSegmentDist(
				const Vector3& p,
				const Segment& segment,
				Vector3& intersectionpoint,
				float& t) {

				float distance = calcPointLineDist(p, segment, intersectionpoint, t);

				// 交点が線分の外にある（始点に近い）
				if (t < 0.0f) {

					intersectionpoint = segment.startpoint;		// 開始点が交点

					float l = (p - intersectionpoint).Length();	// 交点との距離を求める

					return l;
				}

				// 交点が線分の外にある（終点に近い）
				if (t > 1.0f) {

					intersectionpoint = segment.endpoint;		// 終点が交点

					float l = (p - intersectionpoint).Length();	// 交点との距離を求める

					return l;
				}

				// 交点が線分上にある（０＜ｔ＜１）
				return distance;
			}

			// 線分と点の距離を求める方法を使用する
			bool CollisionSphereCylinder(BoundingSphere sphere, BoundingCylinder cylinder)
			{
				// 垂線の足	
				Vector3 intersectionpoint;
				float t{};

				Segment seg;
				seg.startpoint = cylinder.bottom;
				seg.endpoint = cylinder.top;

				// 球の中心と線分の距離を求める
				float length = calcPointSegmentDist(
					sphere.center,
					seg,
					intersectionpoint,
					t);

				// 球の中心と線分の距離が半径より小さい場合は当たっている
				if (length > sphere.radius + cylinder.radius) {
					return false;
				}

				// 線分の始点に近い
				if (t < 0) {
					BoundingBoxAABB aabb;

					aabb.max.x = cylinder.top.x + cylinder.radius;
					aabb.max.y = cylinder.top.y;
					aabb.max.z = cylinder.top.z + cylinder.radius;

					aabb.min.x = cylinder.bottom.x - cylinder.radius;
					aabb.min.y = cylinder.bottom.y;
					aabb.min.z = cylinder.bottom.z - cylinder.radius;

					float length = SqDistPointAABB(sphere.center, aabb);
					length = sqrt(length);

					if (length < sphere.radius) {
						return true;
					}
				}
				// 線分の終点に近い
				else if (t > 1) {
					BoundingBoxAABB aabb;

					aabb.max.x = cylinder.top.x + cylinder.radius;
					aabb.max.y = cylinder.top.y;
					aabb.max.z = cylinder.top.z + cylinder.radius;

					aabb.min.x = cylinder.bottom.x - cylinder.radius;
					aabb.min.y = cylinder.bottom.y;
					aabb.min.z = cylinder.bottom.z - cylinder.radius;

					float length = SqDistPointAABB(sphere.center, aabb);
					length = sqrt(length);

					if (length < sphere.radius) {
						return true;
					}
				}
				// 線分の始点と終点の間にあるか？
				else {
					return true;
				}

				return false;
			}

			// 球
			bool CollisionSphere(BoundingSphere p1, BoundingSphere p2)
			{

				double length = (p1.center - p2.center).Length();

				double radiussum = p1.radius + p2.radius;

				return length <= radiussum;
			}

			// OBBの当たり判定を行う
			bool CollisionOBB(
				const BoundingBoxOBB& obbA,
				const BoundingBoxOBB& obbB) {

				// 課題NO 03 [OBB当たり判定]で完成させてください

				return true;
			}

			// AABB
			bool CollisionAABB(BoundingBoxAABB p1, BoundingBoxAABB p2) {

				// X座標
				if (p1.max.x < p2.min.x) {
					return false;
				}

				if (p1.min.x > p2.max.x) {
					return false;
				}

				// Y座標
				if (p1.max.y < p2.min.y) {
					return false;
				}

				if (p1.min.y > p2.max.y) {
					return false;
				}

				// Z座標
				if (p1.max.z < p2.min.z) {
					return false;
				}

				if (p1.min.z > p2.max.z) {
					return false;
				}

				return true;
			}


			BoundingBoxAABB SetAABB(Vector3 centerposition, float width, float height, float depth) {

				BoundingBoxAABB aabb{};

				width = fabs(width);
				height = fabs(height);
				depth = fabs(depth);

				aabb.min.x = centerposition.x - width / 2.0f;
				aabb.min.y = centerposition.y - height / 2.0f;
				aabb.min.z = centerposition.z - depth / 2.0f;

				aabb.max.x = centerposition.x + width / 2.0f;
				aabb.max.y = centerposition.y + height / 2.0f;
				aabb.max.z = centerposition.z + depth / 2.0f;

				return aabb;
			}

			// AABBと点のもっと近い点を求める
			void ClosestPtPointAABB(Vector3 p, BoundingBoxAABB aabb, Vector3& q)
			{
				q.x = std::max(aabb.min.x, std::min(p.x, aabb.max.x));
				q.y = std::max(aabb.min.y, std::min(p.y, aabb.max.y));
				q.z = std::max(aabb.min.z, std::min(p.z, aabb.max.z));
			}

			// AABBと点の距離を求める（２乗）
			float SqDistPointAABB(Vector3 p, BoundingBoxAABB aabb)
			{
				float sqDist = 0.0f;

				if (p.x < aabb.min.x) sqDist += (aabb.min.x - p.x) * (aabb.min.x - p.x);
				if (p.x > aabb.max.x) sqDist += (p.x - aabb.max.x) * (p.x - aabb.max.x);

				if (p.y < aabb.min.y) sqDist += (aabb.min.y - p.y) * (aabb.min.y - p.y);
				if (p.y > aabb.max.y) sqDist += (p.y - aabb.max.y) * (p.y - aabb.max.y);

				if (p.z < aabb.min.z) sqDist += (aabb.min.z - p.z) * (aabb.min.z - p.z);
				if (p.z > aabb.max.z) sqDist += (p.z - aabb.max.z) * (p.z - aabb.max.z);

				return sqDist;
			}

			// AABBと球の当たり判定
			bool CollisionSphereAABB(
				BoundingSphere sphere,
				BoundingBoxAABB aabb)
			{
				// AABBと点の距離を求める
				float lng = SqDistPointAABB(sphere.center, aabb);

				if (lng < sphere.radius * sphere.radius)
				{
					return true;
				}

				return false;
			}

			// OBBと点のもっと近い点を求める
			void ClosestPtPointOBB(Vector3 point, BoundingBoxOBB obb, Vector3& answer)
			{
				// 点からOBBの中心へのベクトル(差分を求めた)
				Vector3 diff = point - obb.worldcenter;

				answer = obb.worldcenter;

				// OBBのローカル座標に変換（軸は正規化されていることを前提とする）
				float obbaxisx = diff.Dot(obb.axisX);
				float obbaxisy = diff.Dot(obb.axisY);
				float obbaxisz = diff.Dot(obb.axisZ);

				// OBBの各軸に沿った最近接点を求める
				obbaxisx = std::clamp(obbaxisx, -0.5f * obb.lengthx, 0.5f * obb.lengthx);
				obbaxisy = std::clamp(obbaxisy, -0.5f * obb.lengthy, 0.5f * obb.lengthy);
				obbaxisz = std::clamp(obbaxisz, -0.5f * obb.lengthz, 0.5f * obb.lengthz);

				// ワールド座標に変換
				answer = obb.worldcenter + obb.axisX * obbaxisx + obb.axisY * obbaxisy + obb.axisZ * obbaxisz;
			}

			// OBBと点の距離を求める
			float DistPointOBB(Vector3 point, BoundingBoxOBB obb)
			{
				Vector3 answer{};

				// OBBと点のもっと近い点を求める
				ClosestPtPointOBB(point, obb, answer);
				// 距離を求める
				float length = (answer - point).Length();

				return length;
			}

			// OBBの設定
			BoundingBoxOBB SetOBB(
				Vector3 rot,					// 姿勢（回転角度）
				Vector3 pos,					// 中心座標（ワールド）
				float width,					// 幅
				float height,					// 高さ
				float depth)					// 奥行
			{
				BoundingBoxOBB obb{};

				BoundingBoxAABB aabb = SetAABB(pos, width, height, depth);
				obb.max = aabb.max;
				obb.min = aabb.min;

				Matrix4x4 mtx = Matrix4x4::CreateFromYawPitchRoll(rot.y, rot.x, rot.z);

				obb.axisX = Vector3(mtx._11, mtx._12, mtx._13);
				obb.axisY = Vector3(mtx._21, mtx._22, mtx._23);
				obb.axisZ = Vector3(mtx._31, mtx._32, mtx._33);

				obb.lengthx = width;
				obb.lengthy = height;
				obb.lengthz = depth;

				obb.worldcenter = pos;
				obb.center = Vector3(0, 0, 0);

				return obb;
			}

			// OBBと球の当たり判定
			bool CollisionSphereOBB(
				BoundingSphere sphere,
				BoundingBoxOBB obb)
			{
				// OBBと点の距離を求める
				float lng = DistPointOBB(sphere.center, obb);

				if (lng < sphere.radius)
				{
					return true;
				}

				return false;
			}

			// カプセル同士の当たり判定
			bool CollisionCapsule(BoundingCapsule caps1, BoundingCapsule caps2)
			{
				// 線分の始点と終点を求める
				Segment seg1;
				seg1.startpoint = caps1.startpoint;
				seg1.endpoint = caps1.endpoint;

				Segment seg2;
				seg2.startpoint = caps2.startpoint;
				seg2.endpoint = caps2.endpoint;

				// ２つの線分の最短距離を求める
				Vector3 p1, p2;
				float s, t;
				float length = ClosestPtSegmentSegment(seg1, seg2, s, t, p1, p2);

				// 球の半径を考慮して当たり判定
				if (length < caps1.radius + caps2.radius) {
					return true;
				}

				return false;
			}

			// 線分と線分の最短距離を求める
			float ClosestPtSegmentSegment(
				const Segment& seg1,				// 線分１
				const Segment& seg2, 				// 線分２	
				float& s,							// 線分１の最短距離のパラメータ
				float& t,							// 線分２の最短距離のパラメータ
				Vector3& p1, 						// 線分１の最短距離の点	
				Vector3& p2) 						// 線分２の最短距離の点
			{
				Vector3 d1 = seg1.endpoint - seg1.startpoint;	// 線分１の方向
				Vector3 d2 = seg2.endpoint - seg2.startpoint;	// 線分２の方向
				Vector3 r = seg1.startpoint - seg2.startpoint;	// 線分１の始点と線分２の始点を結ぶベクトル

				float a = d1.Dot(d1);	// 線分１の長さの２乗
				float e = d2.Dot(d2);	// 線分２の長さの２乗
				float f = d2.Dot(r);	// 線分２の始点から線分１の始点へのベクトルの長さの２乗

				// 線分１が縮退していないか？　且つ　線分２が縮退していないか？
				if (a <= 1e-5 && e <= 1e-5) {
					s = t = 0.0f;
					p1 = seg1.startpoint;
					p2 = seg2.startpoint;
					return (p1 - p2).Length();		// 縮退している場合は点の距離
				}

				if (a <= 1e-5) {					// 線分１が縮退している場合
					s = 0.0f;
					t = std::clamp(f / e, 0.0f, 1.0f);
				}
				else {
					float c = d1.Dot(r);
					if (e <= 1e-5) {	// 線分２が縮退している場合
						t = 0.0f;
						s = std::clamp(-c / a, 0.0f, 1.0f);
					}
					else {
						// 線分同士の距離を求める
						float b = d1.Dot(d2);

						// 行列式を求める
						float denom = a * e - b * b; // Always nonnegative

						// If segments are not parallel, compute closest point on L1 to L2 and
						// clamp to segment S1. Else pick arbitrary s (here 0)

						if (denom != 0.0f) {
							s = std::clamp((b * f - c * e) / denom, 0.0f, 1.0f);
						}
						else {
							s = 0.0f;
						}

						t = (b * s + f) / e;

						if (t < 0.0f) {
							t = 0.0f;
							s = std::clamp(-c / a, 0.0f, 1.0f);
						}
						else if (t > 1.0f) {
							t = 1.0f;
							s = std::clamp((b - c) / a, 0.0f, 1.0f);
						}
					}
				}

				p1 = seg1.startpoint + d1 * s;
				p2 = seg2.startpoint + d2 * t;
				return (p1 - p2).Length();

			}

// 座標変換した後のBSphereを全頂点から求める

			BoundingSphere calcBSphere(const std::vector<Vector3>& vertices, const SRT transform)
			{
				// 頂点がない場合のフェイルセーフ
				if (vertices.empty()) {
					return BoundingSphere{ Vector3(0,0,0), 0.0f };
				}

				Matrix4x4 mtx = transform.GetMatrix();

				// バグ修正: reserveを使ってメモリ確保のみ行う
				std::vector<Vector3> transformedVertices;
				transformedVertices.reserve(vertices.size());

				// 初期値を極端な値にしておく
				Vector3 minVec = { FLT_MAX, FLT_MAX, FLT_MAX };
				Vector3 maxVec = { -FLT_MAX, -FLT_MAX, -FLT_MAX };

				// 改善点1: 座標変換とAABBの最小/最大値の計算を1つのループに統合
				for (const auto& v : vertices) {
					Vector3 tv = Vector3::Transform(v, mtx);
					transformedVertices.push_back(tv);

					minVec.x = std::min(minVec.x, tv.x);
					minVec.y = std::min(minVec.y, tv.y);
					minVec.z = std::min(minVec.z, tv.z);

					maxVec.x = std::max(maxVec.x, tv.x);
					maxVec.y = std::max(maxVec.y, tv.y);
					maxVec.z = std::max(maxVec.z, tv.z);
				}

				Vector3 center = (minVec + maxVec) / 2.0f;

				// 改善点2: ループ内での重い平方根(sqrt)計算を避けるため、二乗距離で比較する
				float maxRadiusSq = 0.0f;
				for (const auto& tv : transformedVertices) {
					// Vector3に LengthSquared() (二乗距離) があればそれを使うのがベストです
					// 無ければ以下のように計算します
					float dx = tv.x - center.x;
					float dy = tv.y - center.y;
					float dz = tv.z - center.z;
					float distSq = (dx * dx) + (dy * dy) + (dz * dz);

					if (distSq > maxRadiusSq) {
						maxRadiusSq = distSq;
					}
				}

				BoundingSphere bs;
				bs.center = center;
				// 最後に1回だけ平方根を計算する
				bs.radius = std::sqrt(maxRadiusSq);

				return bs;
			}

			// 座標変換した後のAABBを全頂点から求める
			BoundingBoxAABB calcAABB(
				const std::vector<Vector3>& vertices,
				SRT transform)
			{

				Matrix4x4 mtx = Matrix4x4::CreateFromYawPitchRoll(transform.rot.y, transform.rot.x, transform.rot.z);

				std::vector<Vector3> transformedVertices(vertices.size());	

				// 座標変換
				for (const auto& v : vertices) {
					transformedVertices.push_back(Vector3::Transform(v,mtx));
				}

				// AABBの最小値と最大値を求める
				BoundingBoxAABB aabb{};

				Vector3 minVec = transformedVertices.front();
				Vector3 maxVec = transformedVertices.front();

				for (const auto& v : transformedVertices) {
					minVec.x = std::min(minVec.x, v.x);
					minVec.y = std::min(minVec.y, v.y);
					minVec.z = std::min(minVec.z, v.z);

					maxVec.x = std::max(maxVec.x, v.x);
					maxVec.y = std::max(maxVec.y, v.y);
					maxVec.z = std::max(maxVec.z, v.z);
				}

				aabb.min.x = minVec.x;
				aabb.min.y = minVec.y;
				aabb.min.z = minVec.z;
				aabb.max.x = maxVec.x;
				aabb.max.y = maxVec.y;
				aabb.max.z = maxVec.z;

				return aabb;
			}


			// AABBから8頂点を取得
			std::vector<Vector3> GetCorners(const BoundingBoxAABB& box) {
				const auto& min = box.min;
				const auto& max = box.max;

				return {
					Vector3(min.x, min.y, min.z),
					Vector3(max.x, min.y, min.z),
					Vector3(min.x, max.y, min.z),
					Vector3(max.x, max.y, min.z),
					Vector3(min.x, min.y, max.z),
					Vector3(max.x, min.y, max.z),
					Vector3(min.x, max.y, max.z),
					Vector3(max.x, max.y, max.z)
				};
			}

			// 座標変換した後のAABBを求める
			BoundingBoxAABB TransformAABB(
				const BoundingBoxAABB& inAABB,
				SRT transform)
			{

				std::vector<Vector3> corners = GetCorners(inAABB);	// AABBの8頂点を取得

				// SRT情報から行列を取得	
				Matrix4x4 mtx = transform.GetMatrix();

				std::vector<Vector3> transformedVertices{};

				// 座標変換
				for (const auto& v : corners) {
					transformedVertices.push_back(Vector3::Transform(v, mtx));
				}

				// AABBの最小値と最大値を求める
				BoundingBoxAABB aabb{};

				Vector3 minVec = transformedVertices.front();
				Vector3 maxVec = transformedVertices.front();

				for (const auto& v : transformedVertices) {
					minVec.x = std::min(minVec.x, v.x);
					minVec.y = std::min(minVec.y, v.y);
					minVec.z = std::min(minVec.z, v.z);

					maxVec.x = std::max(maxVec.x, v.x);
					maxVec.y = std::max(maxVec.y, v.y);
					maxVec.z = std::max(maxVec.z, v.z);
				}

				aabb.min.x = minVec.x;
				aabb.min.y = minVec.y;
				aabb.min.z = minVec.z;
				aabb.max.x = maxVec.x;
				aabb.max.y = maxVec.y;
				aabb.max.z = maxVec.z;

				return aabb;
			}

			BoundingCapsule SetBoundingCapsule(
				SRT transform,		// カプセルの姿勢情報
				float radius,		// カプセルの半径
				float height)		// カプセルの高さ
			{
				BoundingCapsule boundingcapsule{};

				boundingcapsule.radius = radius;

				// 姿勢を表す行列を作る	
				Matrix4x4 mtx = Matrix4x4::CreateFromYawPitchRoll(
					transform.rot.y,
					transform.rot.x,
					transform.rot.z);

				Vector3 up = mtx.Up();

				boundingcapsule.startpoint = transform.pos;
				boundingcapsule.endpoint = transform.pos + height * up;

				return boundingcapsule;
			}

			BoundingCylinder SetBoundingCylinder(
				SRT transform,		// 円柱の姿勢情報
				Cylinder cylinder)	// 描画用の円柱情報
			{
				BoundingCylinder boundingcylinder{};

				boundingcylinder.radius = cylinder.GetRadius();
				float height = cylinder.GetHeight();

				Matrix4x4 mtx = Matrix4x4::CreateFromYawPitchRoll(
					transform.rot.y,
					transform.rot.x,
					transform.rot.z);

				Vector3 up = mtx.Up();

				boundingcylinder.bottom = transform.pos;
				boundingcylinder.top = transform.pos + height * up;

				return boundingcylinder;
			}

			bool CollisionCylinder(
				const BoundingCylinder& Cylinder1,
				const BoundingCylinder& Cylinder2)
			{
				Segment seg1;
				seg1.startpoint = Cylinder1.bottom;
				seg1.endpoint = Cylinder1.top;

				Segment seg2;
				seg2.startpoint = Cylinder2.bottom;
				seg2.endpoint = Cylinder2.top;

				// 線分と線分の最短距離を求める
				Vector3 p1, p2;
				float s = 0.0f;
				float t = 0.0f;

				float distance = ClosestPtSegmentSegment(
					seg1,				// 線分１
					seg2, 				// 線分２	
					s,					// 線分１の最短距離のパラメータ
					t,					// 線分２の最短距離のパラメータ
					p1, 				// 線分１の最短距離の点	
					p2); 				// 線分２の最短距離の点


				// 直線同士の最短距離が円柱の半径より大きい場合は衝突していない
				if (distance > Cylinder1.radius + Cylinder2.radius) {
					return false;	// 衝突していない
				}

				return true;	// 衝突している
			}

			// 円柱の断面
			struct Cap {
				Vector3 center; // 円の中心位置
				Vector3 normal; // 円の法線ベクトル（＝円柱の軸と同じ）
				float radius;
			};


			bool CapCircleIntersect(const Cap& capA, const Cap& capB) {
				// Step 1: Bの中心をAの法線に射影
				Vector3 delta = capB.center - capA.center;
				float distToPlane = delta.Dot(capA.normal);
				Vector3 projectedB = capB.center - capA.normal * distToPlane;

				// Step 2: Aの中心との平面上距離
				float planarDist = (projectedB - capA.center).Length();

				// Step 3: 半径の合計と比較
				return planarDist <= (capA.radius + capB.radius);
			}

			bool CylinderCapIntersect(
				const BoundingCylinder& c1, 
				const BoundingCylinder& c2) 
			{
				// それぞれの上下端を求める
				Vector3 top1 = c1.top;
				Vector3 top2 = c2.top;

				Vector3 c1axis = c1.top - c1.bottom;
				Vector3 c2axis = c2.top - c2.bottom;

				Cap c1_bottom = { c1.bottom, c1axis, c1.radius };
				Cap c1_top = { top1,         c1axis, c1.radius };
				Cap c2_bottom = { c2.bottom, c2axis, c2.radius };
				Cap c2_top = { top2,         c2axis, c2.radius };

				// 4通りの組み合わせを確認（上下×上下）
				return
					CapCircleIntersect(c1_bottom, c2_bottom) ||
					CapCircleIntersect(c1_bottom, c2_top) ||
					CapCircleIntersect(c1_top, c2_bottom) ||
					CapCircleIntersect(c1_top, c2_top);
			}

			bool PreciseCylinderCollision(
				const BoundingCylinder& Cylinder1,
				const BoundingCylinder& Cylinder2)
			{
				// 線分と線分の最短距離を求める
				Vector3 p1;
				Vector3 p2;
				float s = 0.0f;
				float t = 0.0f;

				Segment seg1;
				seg1.startpoint = Cylinder1.bottom;
				seg1.endpoint = Cylinder1.top;

				Segment seg2;
				seg2.startpoint = Cylinder2.bottom;
				seg2.endpoint = Cylinder2.top;

				float distance = ClosestPtSegmentSegment(
					seg1,				// 線分１
					seg2, 				// 線分２	
					s,					// 線分１の最短距離のパラメータ
					t,					// 線分２の最短距離のパラメータ
					p1, 				// 線分１の最短距離の点	
					p2); 				// 線分２の最短距離の点

				if (distance > Cylinder1.radius * Cylinder2.radius) {
					return false;
				}

				// 精密断面チェック（上下のcap）
				bool sts = CylinderCapIntersect(Cylinder1, Cylinder2);
				if (sts) {
					return true;
				}

				// Step 3: 一方のcapが他方の側面にめり込んでいる
				if (CylinderSideIntersect(Cylinder1, Cylinder2))
					return true;
				if (CylinderSideIntersect(Cylinder2, Cylinder1))
					return true;

				return false;

			}

			bool isPointOnSegment(
				const Segment& seg, 
				const Vector3& point, 
				float epsilon)
			{	
				Vector3 AB = seg.endpoint - seg.startpoint;
				Vector3 AP = point - seg.startpoint;
				Vector3 BP = point - seg.endpoint;

				// 条件1: 同一直線上（クロス積がゼロ）
				Vector3 cross = AB.Cross(AP);
				if (cross.LengthSquared() > epsilon * epsilon) {
					return false; // 同一線上にない
				}

				// 条件2: 内積で範囲を判定
				float dot1 = AB.Dot(AP);
				float dot2 = (-AB).Dot(BP); // = AB.dot(P - B)

				return dot1 >= 0 && dot2 >= 0;
			}

			float ClosestDistanceBetweenLines(
				const Line& line1, 
				const Line& line2,
				float& s, 			// line1 パラメータ（無制限）
				float& t,			// line2 パラメータ（無制限）
				Vector3& p1, 		// line1 最近接点
				Vector3& p2			// line2 最近接点
			) 
			{
				const Vector3& p = line1.point;
				const Vector3& q = line2.point;
				const Vector3& d1 = line1.direction;
				const Vector3& d2 = line2.direction;

				Vector3 r = p - q;

				float a = d1.Dot(d1);       // d1・d1
				float e = d2.Dot(d2);       // d2・d2
				float f = d2.Dot(r);        // d2・(p - q)
				float c = d1.Dot(r);        // d1・(p - q)
				float b = d1.Dot(d2);       // d1・d2
				float denom = a * e - b * b;

				// 並行 or 同一直線
				if (std::abs(denom) < 1e-6f) {
					// 任意 s = 0 にして、t を線形に解く
					s = 0.0f;
					t = f / e;
				}
				else {
					s = (b * f - c * e) / denom;
					t = (a * f - b * c) / denom;
				}

				p1 = p + d1 * s;
				p2 = q + d2 * t;

				return (p1 - p2).Length();
			}

			bool CylinderSideIntersect(
				const BoundingCylinder& cylinderSide, 
				const BoundingCylinder& other)
			{
				// cylinderSideの軸と高さベクトル
				Vector3 axis = cylinderSide.top - cylinderSide.bottom;
				float height = axis.Length();
				Vector3 axisDir = axis / height; // 正規化

				// otherの上下cap
				Vector3 otherCapCenters[2] = { other.bottom, other.top };

				for (const Vector3& point : otherCapCenters) {
					// cylinderSideの軸に点を射影
					Vector3 baseToPoint = point - cylinderSide.bottom;
					float h = baseToPoint.Dot(axisDir); // 射影距離

					// その点がcylinderSideの高さ範囲内にあるか？
					if (h < 0.0f || h > height)
						continue;

					// 軸上の最近接点を求める
					Vector3 closestPoint = cylinderSide.bottom + axisDir * h;

					// 側面までの水平距離を測る
					float radialDist = (point - closestPoint).Length();

					if (radialDist <= cylinderSide.radius + other.radius) {
						return true;
					}
				}

				return false;
			}

			// 直線と平面の交点を求める
			bool LinetoPlaneCross(
				const Plane& plane,		//平面の方程式
				const Vector3& p0,		//直線の起点
				const Vector3& wv,		//直線の方向ベクトル
				float& t,				// 交点位置情報	
				Vector3& ans)			//交点座標
			{
				//	float t = 0;

				Vector3 n;				// 平面の法線ベクトル（正規化済み）
				n.x = plane.a;
				n.y = plane.b;
				n.z = plane.c;

				float dot;			// 分母

				dot = wv.Dot(n);
				// 平行チェック(内積を計算する)
				if (fabsf(dot) < FLT_EPSILON) {
					//		MessageBox(nullptr, "平行です。", "平行です。", MB_OK);

					printf("平行です \n");
					return false;
				}

				// 平面方程式: n・p + d = 0
				t = -(n.Dot(p0) + plane.d) / dot;

				ans = p0 + t * wv;

				return true;
			}

			// 三角形の内部かどうかを判定する
			bool CheckInTriangle(
				const Vector3& a,
				const Vector3& b,
				const Vector3& c,
				const Vector3& p) {

				Vector3	ab, bc, ca;			// ３辺のベクトル
				Vector3	ap, bp, cp;			// 内部の点とのベクトル
				Vector3	normal;				// ３角形の法線ベクトル
				Vector3	n1, n2, n3;			// ３辺と内部の点との法線ベクトル

				ab = b - a;

				bc = c - b;

				ca = a - c;

				ap = p - a;

				bp = p - b;

				cp = p - c;

				normal = ab.Cross(bc);

				n1 = ab.Cross(ap);

				n2 = bc.Cross(bp);

				n3 = ca.Cross(cp);

				float dot = n1.Dot(normal);
				if (dot < 0) return false;			// 為す角度が鈍角

				dot = n2.Dot(normal);
				if (dot < 0) return false;			// 為す角度が鈍角

				dot = n3.Dot(normal);
				if (dot < 0) return false;			// 為す角度が鈍角

				return true;
			}
		}
	}
}