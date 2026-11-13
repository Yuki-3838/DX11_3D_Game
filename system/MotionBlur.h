#pragma once

#include <functional>
#include <memory>
#include <vector>
#include "BoneCombMatrix.h"
#include "CShader.h"
#include "commontypes.h"
#include "renderer.h"

/**
 * @brief キャラクターだけに掛けるモーションブラー。
 *
 * 流れ(1フレーム):
 *   1. BeginVelocityPass … 速度バッファを描画先にする
 *   2. DrawSkinned / DrawRigid … キャラクターをもう一度描き、画面上の移動量を書き込む
 *   3. Apply … 速度バッファをもとに、今の画面へブラーを掛ける
 *
 * 剣の軌跡・火花・UIがぼけないよう、キャラクターを描いた直後、それらを描く前に呼ぶ。
 *
 * カメラの動きは速度に含めない。背景をぼかしていないので、カメラを回しただけで
 * キャラクターだけがぶれると不自然に見えるため。プレイヤーを追うカメラの平行移動は打ち消すので、
 * 走っているプレイヤー(画面上ではほぼ止まっている)は全身ではなく手足と剣がぶれる。
 */
class MotionBlur
{
public:
    /** シェーダーとテクスチャを作る。デバイスしか使わないので裏のスレッドから呼んでもよい。 */
    bool Init();
    /** 前のフレームの記録を捨てる。次に描くフレームはブラーなしになる(場面の切り替わりなど)。 */
    void ResetHistory();

    /**
     * @param followAnchor カメラが追っている点。前のフレームからの移動分を打ち消す。
     */
    void BeginVelocityPass(
        const Matrix4x4& view,
        const Matrix4x4& projection,
        const Vector3& followAnchor);

    /**
     * @brief 骨で動くものを速度バッファへ描く。
     * @param trackId 前のフレームの記録を区別する番号(描くものごとに固定)。
     * @param drawGeometry メッシュを描く処理。シェーダーはここで設定済み。
     */
    void DrawSkinned(
        int trackId,
        const Matrix4x4& world,
        BoneCombMatrix& bones,
        const std::function<void()>& drawGeometry);

    /**
     * @brief 骨を持たないもの(手に持つ剣)を速度バッファへ描く。
     * @param maxRadiusPixels ブラーの片側の長さの上限(ピクセル)。
     *        細いものは長く伸ばすほど薄まって見えなくなるので、全体の上限より短くできる。
     */
    void DrawRigid(
        int trackId,
        const Matrix4x4& world,
        float maxRadiusPixels,
        const std::function<void()>& drawGeometry);

    /** 速度バッファをもとに今の画面へブラーを掛け、描画の状態を元へ戻す。 */
    void Apply();

    float GetStrength() const { return m_strength; }
    void SetStrength(float strength) { m_strength = strength; }
    bool IsDebugViewEnabled() const { return m_debugView; }
    void SetDebugViewEnabled(bool enabled) { m_debugView = enabled; }

private:
    struct Track
    {
        Matrix4x4 previousWorld{};
        bool valid = false;
        // 骨を持つものだけが使う。
        std::unique_ptr<CBBoneCombMatrix> previousBones;
        ComPtr<ID3D11Buffer> previousBoneBuffer;
    };

    Track& GetTrack(int trackId);
    // 今回使う「前のフレームのワールド行列」を求める。比べられないとき(初回や瞬間移動)はfalse。
    bool ResolvePreviousWorld(const Track& track, const Matrix4x4& world, Matrix4x4& outPrevious) const;
    void UploadParams(const Matrix4x4& previousWorld, float maxRadiusPixels);
    float GetMaxRadiusPixels() const { return static_cast<float>(m_tileSize * 2); }

    // シェーダー
    CShader m_velocitySkinShader;
    CShader m_velocityRigidShader;
    ComPtr<ID3D11VertexShader> m_fullscreenVS;
    ComPtr<ID3D11PixelShader> m_tileMaxPS;
    ComPtr<ID3D11PixelShader> m_neighborMaxPS;
    ComPtr<ID3D11PixelShader> m_reconstructPS;

    // 速度バッファ(rg: ブラーの片側の長さ / b: 深度)
    ComPtr<ID3D11RenderTargetView> m_velocityRTV;
    ComPtr<ID3D11ShaderResourceView> m_velocitySRV;
    // タイルごとの最大の速度と、周囲のタイルも含めた最大の速度
    ComPtr<ID3D11RenderTargetView> m_tileMaxRTV;
    ComPtr<ID3D11ShaderResourceView> m_tileMaxSRV;
    ComPtr<ID3D11RenderTargetView> m_neighborMaxRTV;
    ComPtr<ID3D11ShaderResourceView> m_neighborMaxSRV;
    // ブラーを掛ける前の画面の写し(バックバッファは読みながら書けない)
    ComPtr<ID3D11Texture2D> m_sceneTexture;
    ComPtr<ID3D11ShaderResourceView> m_sceneSRV;

    ComPtr<ID3D11Buffer> m_paramsBuffer;
    ComPtr<ID3D11SamplerState> m_linearSampler;
    ComPtr<ID3D11DepthStencilState> m_velocityDepthState;
    ComPtr<ID3D11RasterizerState> m_velocityRasterizer;
    ComPtr<ID3D11RasterizerState> m_fullscreenRasterizer;

    // BeginVelocityPass の時点の描画の状態。Apply で元へ戻す。
    ComPtr<ID3D11VertexShader> m_savedVS;
    ComPtr<ID3D11PixelShader> m_savedPS;
    ComPtr<ID3D11GeometryShader> m_savedGS;
    ComPtr<ID3D11InputLayout> m_savedLayout;
    ComPtr<ID3D11RasterizerState> m_savedRasterizer;
    ComPtr<ID3D11BlendState> m_savedBlendState;
    float m_savedBlendFactor[4]{};
    UINT m_savedBlendMask = 0xffffffff;

    std::vector<std::unique_ptr<Track>> m_tracks;
    Matrix4x4 m_viewProjection{};
    Vector3 m_previousAnchor{};
    Vector3 m_followStep{};
    bool m_anchorValid = false;

    UINT m_width = 0;
    UINT m_height = 0;
    UINT m_tileSize = 16;
    UINT m_tileColumns = 0;
    UINT m_tileRows = 0;
    // 片側の長さ = 1フレームの移動量 × この値。0.5で、ぼけ全体の長さが1フレームの移動量と同じになる。
    // 0.5では尾を振るドラゴンの体の形が読めなくなったので、少し抑えている。
    float m_strength = 0.35f;
    bool m_debugView = false;
    bool m_ready = false;
    bool m_passActive = false;
};
