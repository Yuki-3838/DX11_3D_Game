#pragma once

#include <d3d11.h>
#include <wrl/client.h>

/**
 * @struct DebugColor
 * @brief RGBA color used by the immediate debug renderer.
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
 * @brief Screen-space rectangle in normalized coordinates.
 *
 * Coordinates are expressed as DirectX clip space:
 * x -1 is left, x +1 is right, y -1 is bottom, y +1 is top.
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
 * @brief Tiny DX11 renderer for visible combat debugging.
 *
 * This is intentionally small: it draws colored rectangles without textures,
 * cameras, meshes, or external assets. It gives Phase 2 a visible feedback loop
 * before the real renderer and ImGui backend are expanded.
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
