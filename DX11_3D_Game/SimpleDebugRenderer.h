#pragma once

#include <d3d11.h>
#include <wrl/client.h>

/**
 * @struct DebugColor
 * @brief 即時描画デバッグ表示で使用するRGBAカラー。
 */
struct DebugColor
{
    float r = 1.0f;
    float g = 1.0f;
    float b = 1.0f;
    float a = 1.0f;
};

/**
 * @struct DebugRect
 * @brief 正規化座標で指定する画面上の矩形。
 *
 * DirectXのクリップ空間で表し、x=-1が左、x=1が右、y=-1が下、y=1が上になる。
 */
struct DebugRect
{
    float left = 0.0f;
    float top = 0.0f;
    float right = 0.0f;
    float bottom = 0.0f;
    DebugColor color;
};

/**
 * @class SimpleDebugRenderer
 * @brief 戦闘状態を画面上で確認するための小さなDX11デバッガー。
 *
 * テクスチャ、カメラ、メッシュ、外部アセットを使わず、色付き矩形だけを描画する。
 * 本格的なレンダラーやImGuiデバッグ画面を拡張する前に、処理結果を目視できるようにする。
 */
class SimpleDebugRenderer
{
public:
    bool Init(ID3D11Device* device);
    void DrawRect(ID3D11DeviceContext* context, const DebugRect& rect);

private:
    struct Vertex
    {
        float position[3];
        float color[4];
    };

    Microsoft::WRL::ComPtr<ID3D11VertexShader> m_vertexShader;
    Microsoft::WRL::ComPtr<ID3D11PixelShader> m_pixelShader;
    Microsoft::WRL::ComPtr<ID3D11InputLayout> m_inputLayout;
    Microsoft::WRL::ComPtr<ID3D11Buffer> m_vertexBuffer;
};
