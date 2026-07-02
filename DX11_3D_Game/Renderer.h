#pragma once

#include <DirectXMath.h>
#include <Windows.h>

/**
 * @class Renderer
 * @brief DX11Rendererへ育てるための最小レンダラー境界。
 *
 * Phase 1ではGameAppが直接バックバッファをクリアします。メッシュ描画、影、ポストプロセス、
 * GPU timestamp queryを追加するとき、このクラスへ描画責務を移していきます。
 */
class Renderer
{
public:
    /** @brief 描画対象ウィンドウを保持する。 */
    void SetWindowHandle(HWND windowHandle) { m_windowHandle = windowHandle; }

private:
    HWND m_windowHandle = nullptr;                          ///< DX11のSwapChainに紐づくウィンドウ。
    DirectX::XMMATRIX m_projectionMatrix = DirectX::XMMatrixIdentity(); ///< 三人称カメラ用の射影行列。
};
