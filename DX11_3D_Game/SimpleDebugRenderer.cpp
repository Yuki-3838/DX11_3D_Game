#include "SimpleDebugRenderer.h"

#include <d3dcompiler.h>
#include <cstring>

namespace
{
constexpr char kVertexShaderSource[] =
    "struct VSInput { float3 position : POSITION; float4 color : COLOR; };"
    "struct PSInput { float4 position : SV_POSITION; float4 color : COLOR; };"
    "PSInput main(VSInput input) {"
    "    PSInput output;"
    "    output.position = float4(input.position, 1.0);"
    "    output.color = input.color;"
    "    return output;"
    "}";

constexpr char kPixelShaderSource[] =
    "struct PSInput { float4 position : SV_POSITION; float4 color : COLOR; };"
    "float4 main(PSInput input) : SV_TARGET { return input.color; }";

bool CompileShader(const char* source, const char* entryPoint, const char* target, ID3DBlob** blob)
{
    Microsoft::WRL::ComPtr<ID3DBlob> errors;
    const HRESULT result = D3DCompile(
        source,
        strlen(source),
        nullptr,
        nullptr,
        nullptr,
        entryPoint,
        target,
        0,
        0,
        blob,
        errors.GetAddressOf());

    return SUCCEEDED(result);
}
} // namespace

bool SimpleDebugRenderer::Init(ID3D11Device* device)
{
    Microsoft::WRL::ComPtr<ID3DBlob> vertexShaderBlob;
    Microsoft::WRL::ComPtr<ID3DBlob> pixelShaderBlob;
    if (!CompileShader(kVertexShaderSource, "main", "vs_4_0", vertexShaderBlob.GetAddressOf()))
    {
        return false;
    }

    if (!CompileShader(kPixelShaderSource, "main", "ps_4_0", pixelShaderBlob.GetAddressOf()))
    {
        return false;
    }

    HRESULT result = device->CreateVertexShader(
        vertexShaderBlob->GetBufferPointer(),
        vertexShaderBlob->GetBufferSize(),
        nullptr,
        m_vertexShader.GetAddressOf());
    if (FAILED(result))
    {
        return false;
    }

    result = device->CreatePixelShader(
        pixelShaderBlob->GetBufferPointer(),
        pixelShaderBlob->GetBufferSize(),
        nullptr,
        m_pixelShader.GetAddressOf());
    if (FAILED(result))
    {
        return false;
    }

    D3D11_INPUT_ELEMENT_DESC inputElements[] = {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0},
    };

    result = device->CreateInputLayout(
        inputElements,
        _countof(inputElements),
        vertexShaderBlob->GetBufferPointer(),
        vertexShaderBlob->GetBufferSize(),
        m_inputLayout.GetAddressOf());
    if (FAILED(result))
    {
        return false;
    }

    D3D11_BUFFER_DESC bufferDesc = {};
    bufferDesc.ByteWidth = sizeof(Vertex) * 6;
    bufferDesc.Usage = D3D11_USAGE_DYNAMIC;
    bufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    bufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

    result = device->CreateBuffer(&bufferDesc, nullptr, m_vertexBuffer.GetAddressOf());
    return SUCCEEDED(result);
}

void SimpleDebugRenderer::DrawRect(ID3D11DeviceContext* context, const DebugRect& rect)
{
    const Vertex vertices[] = {
        {{rect.left, rect.top, 0.0f}, {rect.color.r, rect.color.g, rect.color.b, rect.color.a}},
        {{rect.right, rect.top, 0.0f}, {rect.color.r, rect.color.g, rect.color.b, rect.color.a}},
        {{rect.left, rect.bottom, 0.0f}, {rect.color.r, rect.color.g, rect.color.b, rect.color.a}},
        {{rect.left, rect.bottom, 0.0f}, {rect.color.r, rect.color.g, rect.color.b, rect.color.a}},
        {{rect.right, rect.top, 0.0f}, {rect.color.r, rect.color.g, rect.color.b, rect.color.a}},
        {{rect.right, rect.bottom, 0.0f}, {rect.color.r, rect.color.g, rect.color.b, rect.color.a}},
    };

    D3D11_MAPPED_SUBRESOURCE mapped = {};
    if (FAILED(context->Map(m_vertexBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
    {
        return;
    }

    memcpy(mapped.pData, vertices, sizeof(vertices));
    context->Unmap(m_vertexBuffer.Get(), 0);

    const UINT stride = sizeof(Vertex);
    const UINT offset = 0;
    ID3D11Buffer* vertexBuffers[] = {m_vertexBuffer.Get()};
    context->IASetInputLayout(m_inputLayout.Get());
    context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    context->IASetVertexBuffers(0, 1, vertexBuffers, &stride, &offset);
    context->VSSetShader(m_vertexShader.Get(), nullptr, 0);
    context->PSSetShader(m_pixelShader.Get(), nullptr, 0);
    context->Draw(6, 0);
}
