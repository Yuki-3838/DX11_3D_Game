#pragma once
#include	"CommonTypes.h"

/**
 * @struct SRT
 * @brief スケール、回転、平行移動（Translation）情報を格納する構造体
 */
struct SRT {
	Vector3 scale = { 1.0f,1.0f,1.0f };  ///< スケール情報
	Vector3 rot = { 0.0f,0.0f,0.0f };    ///< 回転情報
	Vector3 pos = { 0.0f,0.0f,0.0f };    ///< 平行移動（位置）情報
	Vector3 pivot = { 0.0f,0.0f,0.0f };	 ///< 回転PIVOT情報


	Matrix4x4 GetMatrix() const {
		// 平行移動
		Matrix4x4 tmtx;
		tmtx = Matrix4x4::CreateTranslation(pos.x, pos.y, pos.z);

		// 回転
		Matrix4x4 rmtx;
		rmtx = Matrix4x4::CreateFromYawPitchRoll(rot.y,rot.x,rot.z);

		// ピボット処理
		Matrix4x4 pivotmtx1;
		Matrix4x4 pivotmtx2;
		pivotmtx1 = Matrix4x4::CreateTranslation(-pivot.x, -pivot.y, -pivot.z);
		pivotmtx2 = Matrix4x4::CreateTranslation(pivot.x, pivot.y, pivot.z);

		// 拡大縮小
		Matrix4x4 smtx;
		smtx = Matrix4x4::CreateScale(scale.x, scale.y, scale.z);

		// 合成
		Matrix4x4 mtx = smtx * pivotmtx1 * rmtx * pivotmtx2 * tmtx;

		return mtx;
	}
};

/**
 * @struct SRTQ
 * @brief スケール、回転、平行移動（Translation）情報を格納する構造体の拡張（QUATERNION追加）
 */
struct SRTQ :public SRT{
	Quaternion	quat = { 0.0f,0.0f,0.0f,1.0f };

	SRTQ(){
		scale = Vector3(1, 1, 1);		// scale
		pos = Vector3(0, 0, 0);			// pos
		rot = Vector3(0, 0, 0);			// rot
		quat = Quaternion(0, 0, 0, 1);	// quat
	};
};
