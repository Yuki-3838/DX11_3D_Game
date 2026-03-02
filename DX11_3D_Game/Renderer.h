#pragma once
#pragma once
#include <Windows.h>
#include <DirectXMath.h>
#include <SimpleMath.h>
class Renderer
{
private:
	HWND m_windowHandle;
	DirectX::SimpleMath::Matrix m_projectMatrix;
};
