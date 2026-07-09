#pragma once
#include	<DirectXMath.h>
#include	<wrl/client.h>
#include	<cstdint>
#include	<numbers>
#include	<SimpleMath.h>

// 基本型のエイリアス
using Vector4 = DirectX::SimpleMath::Vector4;
using Vector3 = DirectX::SimpleMath::Vector3;
using Vector2 = DirectX::SimpleMath::Vector2;

using Matrix4x4 = DirectX::SimpleMath::Matrix;

using Color = DirectX::SimpleMath::Color;

using Quaternion = DirectX::SimpleMath::Quaternion;

using Microsoft::WRL::ComPtr;

constexpr float PI = std::numbers::pi_v<float>;
