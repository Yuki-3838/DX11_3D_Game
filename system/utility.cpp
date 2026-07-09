#include    "CommonTypes.h"
#include	"utility.h"
#include	<filesystem>
#include	<string>
#include	<Windows.h>
#include	<random>
#include	<assimp/scene.h>

namespace utility {
    // std::string 用のディレクトリ取得関数
    std::filesystem::path get_directory(const std::string& path) {
        return std::filesystem::path(path).parent_path();
    }

    // std::u8string 用のディレクトリ取得関数
    std::filesystem::path get_directory(const std::u8string& path) {
        return std::filesystem::path(path).parent_path();
    }

    // std::wstring 用のディレクトリ取得関数
    std::filesystem::path get_directory(const std::wstring& path) {
        return std::filesystem::path(path).parent_path();
    }

	// ワイド文字(utf16)をｓ－ｊｉｓに
	std::string wide_to_multi_winapi(std::wstring const& src)
	{
		auto const dest_size = ::WideCharToMultiByte(
			CP_ACP,
			0U,
			src.data(),
			-1,
			nullptr,
			0,
			nullptr,
			nullptr);
		std::vector<char> dest(dest_size, '\0');
		if (::WideCharToMultiByte(
			CP_ACP,
			0U,
			src.data(),
			-1,
			dest.data(),
			static_cast<int>(dest.size()),
			nullptr,
			nullptr) == 0) {
			throw std::system_error{ static_cast<int>(::GetLastError()), std::system_category() };
		}
		dest.resize(std::char_traits<char>::length(dest.data()));
		dest.shrink_to_fit();
		return std::string(dest.begin(), dest.end());
	}

	// utf-8をワイド文字(utf-16)に
	std::wstring utf8_to_wide_winapi(std::string const& src)
	{
		auto const dest_size = ::MultiByteToWideChar(
			CP_UTF8,			 // ソース側がUTF-8
			0U,
			src.data(),
			-1,
			nullptr,
			0U);
		std::vector<wchar_t> dest(dest_size, L'\0');
		if (::MultiByteToWideChar(CP_UTF8, 0U, src.data(), -1, dest.data(), static_cast<int>(dest.size())) == 0) {
			throw std::system_error{ static_cast<int>(::GetLastError()), std::system_category() };
		}
		dest.resize(std::char_traits<wchar_t>::length(dest.data()));
		dest.shrink_to_fit();
		return std::wstring(dest.begin(), dest.end());
	}

	// utf8をS-JISに
	std::string utf8_to_multi_winapi(std::string const& src)
	{
		auto const wide = utf8_to_wide_winapi(src);
		return wide_to_multi_winapi(wide);
	}


	Matrix4x4 aiMtxToDxMtx(aiMatrix4x4 asimpmtx) 
	{
		Matrix4x4 dxmtx;

		dxmtx._11 = asimpmtx.a1; dxmtx._12 = asimpmtx.b1; dxmtx._13 = asimpmtx.c1; dxmtx._14 = asimpmtx.d1;
		dxmtx._21 = asimpmtx.a2; dxmtx._22 = asimpmtx.b2; dxmtx._23 = asimpmtx.c2; dxmtx._24 = asimpmtx.d2;
		dxmtx._31 = asimpmtx.a3; dxmtx._32 = asimpmtx.b3; dxmtx._33 = asimpmtx.c3; dxmtx._34 = asimpmtx.d3;
		dxmtx._41 = asimpmtx.a4; dxmtx._42 = asimpmtx.b4; dxmtx._43 = asimpmtx.c4; dxmtx._44 = asimpmtx.d4;
		return dxmtx;
	}

	Matrix4x4 CaliculateBillBoardMtx(Matrix4x4 mtxView)
	{
		// ワールドマトリックスの初期化
		Matrix4x4 mtxWorld = Matrix4x4::Identity;

		// ビルボード用行列作成
		mtxWorld._11 = mtxView._11;
		mtxWorld._12 = mtxView._21;
		mtxWorld._13 = mtxView._31;
		mtxWorld._21 = mtxView._12;
		mtxWorld._22 = mtxView._22;
		mtxWorld._23 = mtxView._32;
		mtxWorld._31 = mtxView._13;
		mtxWorld._32 = mtxView._23;
		mtxWorld._33 = mtxView._33;

		return mtxWorld;
	}

	// ターゲットの方向を向くクオータニオンを作成する関数(上固定なし)
	Quaternion CreateTargetQuaternion(
		const Vector3 start,
		const Vector3 end)
	{

		Vector3 normalizedStart = start;
		Vector3 normalizedEnd = end;

		normalizedStart.Normalize();
		normalizedEnd.Normalize();

		Vector3 cross = normalizedStart.Cross(normalizedEnd);
		float dotFloat = normalizedStart.Dot(normalizedEnd);

		if (dotFloat < -0.999999f) {
			// ベクトルがほぼ反対方向の場合
			// 任意の直交ベクトルを軸として180度回転
			Vector3 orthogonal = normalizedStart.Cross(Vector3(1.0f, 0.0f, 0.0f));

			if (orthogonal.LengthSquared() < 0.1f) {
				orthogonal = Vector3(0.0f, 1.0f, 0.0f);
			}
			orthogonal.Normalize();
			return Quaternion::CreateFromAxisAngle(orthogonal, DirectX::XM_PI);
		}

		if (dotFloat > 0.999999f) {
			// ベクトルがほぼ同じ方向の場合
			// 任意の直交ベクトルを軸として0度回転
			Vector3 orthogonal = normalizedStart.Cross(Vector3(1.0f, 0.0f, 0.0f));

			if (orthogonal.LengthSquared() < 0.1f) {
				orthogonal = Vector3(0.0f, 1.0f, 0.0f);
			}
			orthogonal.Normalize();
			return Quaternion::CreateFromAxisAngle(orthogonal, 0.0f);
		}

		float angle = acosf(dotFloat);
		cross.Normalize();
		return Quaternion::CreateFromAxisAngle(cross, angle);
	}

	// ターゲットの方向を向くクオータニオンを作成する関数(上固定あり)
	Quaternion CreateLookatQuaternion(
		const Vector3 start,
		const Vector3 end,
		const Vector3 up,
		Matrix4x4& mtx)
	{

		Vector3 axisx{};
		Vector3 axisy{};
		Vector3 axisz=(end-start);

		// 外積でX軸を求める
		axisx = up.Cross(axisz);

		// 外積でY軸を求める
		axisy = axisz.Cross(axisx);

		// 正規化
		axisx.Normalize();
		axisy.Normalize();
		axisz.Normalize();

		mtx = Matrix4x4::Identity;

		mtx._11 = axisx.x;
		mtx._12 = axisx.y;
		mtx._13 = axisx.z;

		mtx._21 = axisy.x;
		mtx._22 = axisy.y;
		mtx._23 = axisy.z;

		mtx._31 = axisz.x;
		mtx._32 = axisz.y;
		mtx._33 = axisz.z;

		return Quaternion::CreateFromRotationMatrix(mtx);
	}

	Vector3 UnprojectSimpleMath(
		const Vector3& screenPos,
		float viewportX, float viewportY,
		float viewportWidth, float viewportHeight,
		float minZ, float maxZ,
		const Matrix4x4& projection,
		const Matrix4x4& view,
		const Matrix4x4& world)
	{
		DirectX::XMVECTOR v = DirectX::XMVectorSet(screenPos.x, screenPos.y, screenPos.z, 1.0f);

		v = DirectX::XMVector3Unproject(
			v,
			viewportX, viewportY,
			viewportWidth, viewportHeight,
			minZ, maxZ,
			projection,
			view,
			world
		);

		Vector3 worldPos;
		XMStoreFloat3(&worldPos, v);
	
		return worldPos;
	}
	
	std::vector<Vector3> GenerateRandomPointInCircle(
		float radius,						// 半径	
		unsigned int num) {					// 発生させる個数

		std::vector<Vector3> points{};

		std::random_device rd;
		std::mt19937	mt(rd());

		std::uniform_real_distribution<float> distazumith(0, 2 * PI);
		std::uniform_real_distribution<float> distradius(radius * 0.1f, radius);

		points.resize(num);

		for (auto& p : points) {
			float azumith = distazumith(mt);	// 角度を0から2πでランダムに選択
			float radius = distradius(mt);		// 半径をランダムに選択

			p.x = radius * cosf(azumith);
			p.z = radius * sinf(azumith);
			p.y = 0.0f;
		}

		return points;
	}

	std::vector<Vector3> GeneratePointsInsideCone(
		float radius,					// 半径
		float height,					// 高さ
		unsigned int num				// 個数
	)
	{
		std::vector<Vector3> points{};

		std::random_device rd;
		std::mt19937	mt(rd());

		std::uniform_real_distribution<float> distheight(height * 0.1f, height);
		points = GenerateRandomPointInCircle(radius, num);

		// 指定の高さをセット
		for (auto& p : points)
		{
			p.y = distheight(mt);
			// 指定した高さの際の半径内に抑える
			// 比例計算
			// height : radius = p.y : r
			float r = radius * p.y / height;
			p.x = p.x * r / radius;
			p.z = p.z * r / radius;
		}

		return points;
	}

	// Windows: UTF-16(wstring) -> UTF-8(string)
	std::string WideToUtf8(const std::wstring& ws)
	{
		if (ws.empty()) return {};
		const int sizeNeeded = ::WideCharToMultiByte(
			CP_UTF8, 0,
			ws.data(), (int)ws.size(),
			nullptr, 0,
			nullptr, nullptr);

		std::string result(sizeNeeded, '\0');
		::WideCharToMultiByte(
			CP_UTF8, 0,
			ws.data(), (int)ws.size(),
			result.data(), sizeNeeded,
			nullptr, nullptr);
		return result;
	}

	std::string PathToUtf8String(const std::filesystem::path& p)
	{
		// Windowsはこれが一番確実（日本語含めて安定）
		return WideToUtf8(p.wstring());
	}
}
