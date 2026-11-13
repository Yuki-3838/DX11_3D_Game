#include "MotionBlur.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include "../Application.h"
#include "dx11helper.h"

namespace
{
    constexpr const char* POST_SHADER_FILE = "shader/motionBlurPost.hlsl";
    // シェーダー側で使うテクスチャの番号(t0はマテリアル、t1は影)。
    constexpr UINT VELOCITY_SRV_SLOT = 2;
    constexpr UINT TILE_SRV_SLOT = 3;
    constexpr UINT SCENE_SRV_SLOT = 4;
    // サンプラーの番号(s0はマテリアル、s1は影)。
    constexpr UINT LINEAR_SAMPLER_SLOT = 2;
    constexpr UINT PREVIOUS_BONE_CB_SLOT = 9;
    constexpr UINT PARAMS_CB_SLOT = 10;

    // 背景の深度(動いているものより必ず奥になる値)。カメラの描画距離と同じ。
    constexpr float BACKGROUND_DEPTH = 10000.0f;
    // カメラの追従として打ち消す1フレームの移動の上限(ワールド単位)。
    // これを超えるのは、演出の後に追従点を取り直したときの飛びで、カメラが実際に動いた量ではない。
    constexpr float MAX_FOLLOW_STEP = 12.0f;
    // 1フレームで画面の4分の1(クリップ空間で0.5)以上動いたら瞬間移動として扱う。
    // 本当に動いたわけではないので、画面の端まで伸びる筋を出さない。
    constexpr float TELEPORT_CLIP_DISTANCE = 0.5f;

    // シェーダー側の cbuffer MotionBlurParams(b10)と対応させる。
    struct MotionBlurParams
    {
        Matrix4x4 PreviousWorld;
        float ViewportWidth;
        float ViewportHeight;
        float BlurStrength;
        float MaxBlurRadius;
        int TileSize;
        int DebugView;
        int Padding[2];
    };

    bool CreateColorTarget(
        UINT width,
        UINT height,
        DXGI_FORMAT format,
        ComPtr<ID3D11RenderTargetView>& outRTV,
        ComPtr<ID3D11ShaderResourceView>& outSRV)
    {
        ID3D11Device* device = Renderer::GetDevice();
        D3D11_TEXTURE2D_DESC desc{};
        desc.Width = width;
        desc.Height = height;
        desc.MipLevels = 1;
        desc.ArraySize = 1;
        desc.Format = format;
        desc.SampleDesc.Count = 1;
        desc.Usage = D3D11_USAGE_DEFAULT;
        desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
        ComPtr<ID3D11Texture2D> texture;
        return SUCCEEDED(device->CreateTexture2D(&desc, nullptr, texture.GetAddressOf())) &&
            SUCCEEDED(device->CreateRenderTargetView(texture.Get(), nullptr, outRTV.GetAddressOf())) &&
            SUCCEEDED(device->CreateShaderResourceView(texture.Get(), nullptr, outSRV.GetAddressOf()));
    }

    void SetViewport(UINT width, UINT height)
    {
        D3D11_VIEWPORT viewport{};
        viewport.Width = static_cast<float>(width);
        viewport.Height = static_cast<float>(height);
        viewport.MaxDepth = 1.0f;
        Renderer::GetDeviceContext()->RSSetViewports(1, &viewport);
    }

    void UploadBuffer(ID3D11Buffer* buffer, const void* data, size_t size)
    {
        ID3D11DeviceContext* context = Renderer::GetDeviceContext();
        D3D11_MAPPED_SUBRESOURCE mapped{};
        if (SUCCEEDED(context->Map(buffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
        {
            std::memcpy(mapped.pData, data, size);
            context->Unmap(buffer, 0);
        }
    }
}

bool MotionBlur::Init()
{
    m_ready = false;
    ID3D11Device* device = Renderer::GetDevice();
    m_width = Application::GetWidth();
    m_height = Application::GetHeight();
    // ブラーの上限はタイル2枚分。解像度が変わっても、画面に対するぼけの長さをそろえる。
    m_tileSize = std::max(8u, m_height / 45u);
    m_tileColumns = (m_width + m_tileSize - 1) / m_tileSize;
    m_tileRows = (m_height + m_tileSize - 1) / m_tileSize;

    m_velocitySkinShader.Create(
        "shader/motionBlurVelocitySkinVS.hlsl",
        "shader/motionBlurVelocityPS.hlsl");
    m_velocityRigidShader.Create(
        "shader/motionBlurVelocityVS.hlsl",
        "shader/motionBlurVelocityPS.hlsl");

    // 全画面の頂点シェーダーは入力を持たないので、入力レイアウトも作る CreateVertexShader は使えない。
    ComPtr<ID3DBlob> fullscreenCode;
    if (FAILED(CompileShaderFromFile(POST_SHADER_FILE, "VSFullscreen", "vs_5_0", fullscreenCode.GetAddressOf())) ||
        FAILED(device->CreateVertexShader(
            fullscreenCode->GetBufferPointer(),
            fullscreenCode->GetBufferSize(),
            nullptr,
            m_fullscreenVS.GetAddressOf())))
    {
        return false;
    }
    if (!CreatePixelShader(device, POST_SHADER_FILE, "PSTileMax", "ps_5_0", m_tileMaxPS.GetAddressOf()) ||
        !CreatePixelShader(device, POST_SHADER_FILE, "PSNeighborMax", "ps_5_0", m_neighborMaxPS.GetAddressOf()) ||
        !CreatePixelShader(device, POST_SHADER_FILE, "PSReconstruct", "ps_5_0", m_reconstructPS.GetAddressOf()))
    {
        return false;
    }

    if (!CreateColorTarget(m_width, m_height, DXGI_FORMAT_R16G16B16A16_FLOAT, m_velocityRTV, m_velocitySRV) ||
        !CreateColorTarget(m_tileColumns, m_tileRows, DXGI_FORMAT_R16G16_FLOAT, m_tileMaxRTV, m_tileMaxSRV) ||
        !CreateColorTarget(m_tileColumns, m_tileRows, DXGI_FORMAT_R16G16_FLOAT, m_neighborMaxRTV, m_neighborMaxSRV))
    {
        return false;
    }

    // 画面の写し。形式はバックバッファと同じにする(CopyResourceの条件)。
    D3D11_TEXTURE2D_DESC sceneDesc{};
    {
        ComPtr<ID3D11Texture2D> backBuffer;
        if (FAILED(Renderer::GetSwapChain()->GetBuffer(
                0, __uuidof(ID3D11Texture2D), reinterpret_cast<void**>(backBuffer.GetAddressOf()))))
        {
            return false;
        }
        backBuffer->GetDesc(&sceneDesc);
    }
    sceneDesc.Usage = D3D11_USAGE_DEFAULT;
    sceneDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    sceneDesc.CPUAccessFlags = 0;
    sceneDesc.MiscFlags = 0;
    if (FAILED(device->CreateTexture2D(&sceneDesc, nullptr, m_sceneTexture.GetAddressOf())) ||
        FAILED(device->CreateShaderResourceView(m_sceneTexture.Get(), nullptr, m_sceneSRV.GetAddressOf())))
    {
        return false;
    }

    if (!CreateConstantBufferWrite(device, sizeof(MotionBlurParams), m_paramsBuffer.GetAddressOf()))
        return false;

    D3D11_SAMPLER_DESC samplerDesc{};
    samplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
    samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
    samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    samplerDesc.MaxLOD = D3D11_FLOAT32_MAX;
    if (FAILED(device->CreateSamplerState(&samplerDesc, m_linearSampler.GetAddressOf())))
        return false;

    // 速度は見えている面にだけ書く(本描画の深度で隠れた部分を落とす)。深度は書き換えない。
    D3D11_DEPTH_STENCIL_DESC depthDesc{};
    depthDesc.DepthEnable = TRUE;
    depthDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
    depthDesc.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
    if (FAILED(device->CreateDepthStencilState(&depthDesc, m_velocityDepthState.GetAddressOf())))
        return false;

    // 本描画と同じ面を描くので深度はほぼ同じ値になる。丸め誤差で負けて穴があかないよう、少しだけ手前へ寄せる。
    D3D11_RASTERIZER_DESC rasterizerDesc{};
    rasterizerDesc.FillMode = D3D11_FILL_SOLID;
    rasterizerDesc.CullMode = D3D11_CULL_BACK;
    rasterizerDesc.DepthClipEnable = TRUE;
    rasterizerDesc.DepthBias = -200;
    if (FAILED(device->CreateRasterizerState(&rasterizerDesc, m_velocityRasterizer.GetAddressOf())))
        return false;

    rasterizerDesc.CullMode = D3D11_CULL_NONE;
    rasterizerDesc.DepthBias = 0;
    if (FAILED(device->CreateRasterizerState(&rasterizerDesc, m_fullscreenRasterizer.GetAddressOf())))
        return false;

    m_ready = true;
    return true;
}

void MotionBlur::ResetHistory()
{
    for (auto& track : m_tracks)
        track->valid = false;
    m_anchorValid = false;
}

MotionBlur::Track& MotionBlur::GetTrack(int trackId)
{
    const size_t index = static_cast<size_t>(trackId);
    if (index >= m_tracks.size())
        m_tracks.resize(index + 1);
    if (!m_tracks[index])
        m_tracks[index] = std::make_unique<Track>();
    return *m_tracks[index];
}

void MotionBlur::UploadParams(const Matrix4x4& previousWorld, float maxRadiusPixels)
{
    MotionBlurParams params{};
    // シェーダーへは転置して渡す(このプロジェクトの他の行列と同じ扱い)。
    params.PreviousWorld = previousWorld.Transpose();
    params.ViewportWidth = static_cast<float>(m_width);
    params.ViewportHeight = static_cast<float>(m_height);
    params.BlurStrength = m_strength;
    // タイルで探せる範囲を超えないようにする。
    params.MaxBlurRadius = std::min(maxRadiusPixels, GetMaxRadiusPixels());
    params.TileSize = static_cast<int>(m_tileSize);
    params.DebugView = m_debugView ? 1 : 0;
    UploadBuffer(m_paramsBuffer.Get(), &params, sizeof(params));
}

void MotionBlur::BeginVelocityPass(
    const Matrix4x4& view,
    const Matrix4x4& projection,
    const Vector3& followAnchor)
{
    if (!m_ready)
        return;
    ID3D11DeviceContext* context = Renderer::GetDeviceContext();

    m_viewProjection = view * projection;
    m_followStep = m_anchorValid ? followAnchor - m_previousAnchor : Vector3(0.0f, 0.0f, 0.0f);
    if (m_followStep.Length() > MAX_FOLLOW_STEP)
        m_followStep = Vector3(0.0f, 0.0f, 0.0f);
    m_previousAnchor = followAnchor;
    m_anchorValid = true;

    context->VSGetShader(m_savedVS.ReleaseAndGetAddressOf(), nullptr, nullptr);
    context->PSGetShader(m_savedPS.ReleaseAndGetAddressOf(), nullptr, nullptr);
    context->GSGetShader(m_savedGS.ReleaseAndGetAddressOf(), nullptr, nullptr);
    context->IAGetInputLayout(m_savedLayout.ReleaseAndGetAddressOf());
    context->RSGetState(m_savedRasterizer.ReleaseAndGetAddressOf());
    context->OMGetBlendState(m_savedBlendState.ReleaseAndGetAddressOf(), m_savedBlendFactor, &m_savedBlendMask);

    // 速度0・背景の深度で消す。
    const float clearValue[4] = { 0.0f, 0.0f, BACKGROUND_DEPTH, 0.0f };
    context->ClearRenderTargetView(m_velocityRTV.Get(), clearValue);
    context->OMSetRenderTargets(1, m_velocityRTV.GetAddressOf(), Renderer::GetMainDepthStencilView());
    SetViewport(m_width, m_height);
    context->OMSetDepthStencilState(m_velocityDepthState.Get(), 0);
    context->RSSetState(m_velocityRasterizer.Get());
    Renderer::SetBlendState(BS_NONE);

    context->VSSetConstantBuffers(PARAMS_CB_SLOT, 1, m_paramsBuffer.GetAddressOf());
    context->PSSetConstantBuffers(PARAMS_CB_SLOT, 1, m_paramsBuffer.GetAddressOf());
    // 何も描かなくても Apply で使う値が入っているようにする。
    UploadParams(Matrix4x4::Identity, GetMaxRadiusPixels());
    m_passActive = true;
}

bool MotionBlur::ResolvePreviousWorld(
    const Track& track,
    const Matrix4x4& world,
    Matrix4x4& outPrevious) const
{
    if (!track.valid)
        return false;
    outPrevious = track.previousWorld * Matrix4x4::CreateTranslation(m_followStep);

    const Vector4 current = Vector4::Transform(
        Vector4(world._41, world._42, world._43, 1.0f), m_viewProjection);
    const Vector4 previous = Vector4::Transform(
        Vector4(outPrevious._41, outPrevious._42, outPrevious._43, 1.0f), m_viewProjection);
    if (current.w <= 0.01f || previous.w <= 0.01f)
        return true;
    const Vector2 moved(
        current.x / current.w - previous.x / previous.w,
        current.y / current.w - previous.y / previous.w);
    return moved.Length() < TELEPORT_CLIP_DISTANCE;
}

void MotionBlur::DrawSkinned(
    int trackId,
    const Matrix4x4& world,
    BoneCombMatrix& bones,
    const std::function<void()>& drawGeometry)
{
    if (!m_passActive)
        return;
    ID3D11DeviceContext* context = Renderer::GetDeviceContext();
    Track& track = GetTrack(trackId);
    if (!track.previousBones)
    {
        track.previousBones = std::make_unique<CBBoneCombMatrix>();
        if (!CreateConstantBufferWrite(
                Renderer::GetDevice(), sizeof(CBBoneCombMatrix), track.previousBoneBuffer.GetAddressOf()))
        {
            return;
        }
    }

    // 比べられないときは「今と同じ」にする。移動量0になり、そのフレームはブラーが掛からない。
    Matrix4x4 previousWorld = world;
    const bool hasHistory = ResolvePreviousWorld(track, world, previousWorld);
    if (!hasHistory)
        previousWorld = world;
    UploadParams(previousWorld, GetMaxRadiusPixels());
    UploadBuffer(
        track.previousBoneBuffer.Get(),
        hasHistory ? track.previousBones.get() : &bones.ConstantBufferMemory,
        sizeof(CBBoneCombMatrix));

    Matrix4x4 currentWorld = world;
    Renderer::SetWorldMatrix(&currentWorld);
    m_velocitySkinShader.SetGPU();
    bones.SetGPU();
    context->VSSetConstantBuffers(PREVIOUS_BONE_CB_SLOT, 1, track.previousBoneBuffer.GetAddressOf());
    drawGeometry();

    *track.previousBones = bones.ConstantBufferMemory;
    track.previousWorld = world;
    track.valid = true;
}

void MotionBlur::DrawRigid(
    int trackId,
    const Matrix4x4& world,
    float maxRadiusPixels,
    const std::function<void()>& drawGeometry)
{
    if (!m_passActive)
        return;
    Track& track = GetTrack(trackId);
    Matrix4x4 previousWorld = world;
    if (!ResolvePreviousWorld(track, world, previousWorld))
        previousWorld = world;
    UploadParams(previousWorld, maxRadiusPixels);

    Matrix4x4 currentWorld = world;
    Renderer::SetWorldMatrix(&currentWorld);
    m_velocityRigidShader.SetGPU();
    drawGeometry();

    track.previousWorld = world;
    track.valid = true;
}

void MotionBlur::Apply()
{
    if (!m_passActive)
        return;
    m_passActive = false;
    ID3D11DeviceContext* context = Renderer::GetDeviceContext();
    ID3D11RenderTargetView* nullTarget = nullptr;
    ID3D11ShaderResourceView* nullViews[3] = { nullptr, nullptr, nullptr };

    // 全画面の三角形を描く準備。頂点は頂点番号から作るので、頂点バッファは使わない。
    context->OMSetRenderTargets(1, &nullTarget, nullptr);
    context->IASetInputLayout(nullptr);
    context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    context->VSSetShader(m_fullscreenVS.Get(), nullptr, 0);
    context->GSSetShader(nullptr, nullptr, 0);
    context->RSSetState(m_fullscreenRasterizer.Get());
    Renderer::SetDepthEnable(false);

    // タイルごとの最大の速度
    SetViewport(m_tileColumns, m_tileRows);
    context->OMSetRenderTargets(1, m_tileMaxRTV.GetAddressOf(), nullptr);
    context->PSSetShader(m_tileMaxPS.Get(), nullptr, 0);
    context->PSSetShaderResources(VELOCITY_SRV_SLOT, 1, m_velocitySRV.GetAddressOf());
    context->Draw(3, 0);

    // 周囲のタイルも含めた最大の速度
    context->OMSetRenderTargets(1, m_neighborMaxRTV.GetAddressOf(), nullptr);
    context->PSSetShader(m_neighborMaxPS.Get(), nullptr, 0);
    context->PSSetShaderResources(TILE_SRV_SLOT, 1, m_tileMaxSRV.GetAddressOf());
    context->Draw(3, 0);
    context->OMSetRenderTargets(1, &nullTarget, nullptr);

    // 今の画面を写してから、ブラーを掛けた色で上書きする。
    ComPtr<ID3D11Texture2D> backBuffer;
    if (SUCCEEDED(Renderer::GetSwapChain()->GetBuffer(
            0, __uuidof(ID3D11Texture2D), reinterpret_cast<void**>(backBuffer.GetAddressOf()))))
    {
        context->CopyResource(m_sceneTexture.Get(), backBuffer.Get());

        ID3D11RenderTargetView* mainTarget = Renderer::GetMainRenderTargetView();
        SetViewport(m_width, m_height);
        context->OMSetRenderTargets(1, &mainTarget, nullptr);
        context->PSSetShader(m_reconstructPS.Get(), nullptr, 0);
        context->PSSetShaderResources(TILE_SRV_SLOT, 1, m_neighborMaxSRV.GetAddressOf());
        context->PSSetShaderResources(SCENE_SRV_SLOT, 1, m_sceneSRV.GetAddressOf());
        context->PSSetSamplers(LINEAR_SAMPLER_SLOT, 1, m_linearSampler.GetAddressOf());
        context->Draw(3, 0);
    }

    // 元へ戻す。テクスチャは外しておかないと、次のフレームに描画先として使えない。
    context->PSSetShaderResources(VELOCITY_SRV_SLOT, 3, nullViews);
    Renderer::RestoreMainRenderTarget();
    Renderer::SetDepthEnable(true);
    context->RSSetState(m_savedRasterizer.Get());
    context->OMSetBlendState(m_savedBlendState.Get(), m_savedBlendFactor, m_savedBlendMask);
    context->VSSetShader(m_savedVS.Get(), nullptr, 0);
    context->PSSetShader(m_savedPS.Get(), nullptr, 0);
    context->GSSetShader(m_savedGS.Get(), nullptr, 0);
    context->IASetInputLayout(m_savedLayout.Get());
}
