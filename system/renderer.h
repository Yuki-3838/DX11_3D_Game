#pragma once

/**
 * @file Renderer.h
 * @brief DirectXレンダリングで使用する各種構造体とRendererクラスの宣言
 */

#include "CommonTypes.h"
#include "NonCopyable.h"
#include <d3d11.h>
#include <io.h>
#include <string>
#include <vector>
#include <wrl/client.h>

 // リンクすべき外部ライブラリ

#pragma comment(lib,"d3d11.lib")

/**
 * @struct WEIGHT
 * @brief ボーンの影響情報を保持する構造体
 * @date 20231225 追加
 */
struct WEIGHT {
    std::string bonename;   ///< ボーン名
    std::string meshname;   ///< メッシュ名
    float weight;           ///< ウェイト値
    int vertexindex;        ///< 頂点インデックス
};

/**
 * @struct BONE
 * @brief ボーン構造体（DX対応版）
 * @date 20231231 修正（DX化）
 */
struct BONE
{
    std::string bonename;          ///< ボーン名
    std::string meshname;          ///< メッシュ名
    std::string armaturename;      ///< アーマチュア名
    Matrix4x4 Matrix{};            ///< 親子関係を考慮した行列
    Matrix4x4 AnimationMatrix{};   ///< 自分の変形のみを考慮した行列
    Matrix4x4 OffsetMatrix{};      ///< ボーンオフセット行列
    int idx=0;                     ///< 配列中のインデックス
    std::vector<WEIGHT> weights;   ///< このボーンが影響を与える頂点とウェイト値のリスト
};

/**
 * @struct VERTEX_3D
 * @brief ３次元頂点データを格納する構造体
 */
struct VERTEX_3D
{
    Vector3 Position;            ///< 頂点の座標
    Vector3 Normal;              ///< 法線ベクトル
    Color Diffuse;               ///< 拡散反射色
    Vector2 TexCoord;            ///< テクスチャ座標
    int BoneIndex[4];            ///< ボーンインデックス（最大4つ） 20231225
    float BoneWeight[4];         ///< 各ボーンのウェイト値 20231225
    std::string BoneName[4];     ///< 各ボーンの名前 20231226
    int bonecnt = 0;             ///< 影響を与えるボーン数 20231226
};

/**
 * @struct MATERIAL
 * @brief マテリアル情報を保持する構造体
 */
struct MATERIAL
{
    Color Ambient;         ///< アンビエント色
    Color Diffuse;         ///< 拡散反射色
    Color Specular;        ///< 鏡面反射色
    Color Emission;        ///< 自己発光色
    float Shiness=0.0f;    ///< 光沢度
    BOOL TextureEnable=FALSE;    ///< テクスチャ使用フラグ
    float Dummy[2]{};      ///< 予備領域
};

/**
 * @struct LIGHT
 * @brief 平行光源の情報を保持する構造体
 */
struct LIGHT
{
    BOOL Enable;           ///< ライトの有効/無効フラグ
    BOOL Dummy[3];         ///< パディング用（ダミー）
    Vector4 Direction;     ///< 光の方向
    Color Diffuse;         ///< 拡散光の色
    Color Ambient;         ///< 環境光の色
};

/**
 * @struct SUBSET
 * @brief メッシュのサブセット（マテリアル毎）情報を保持する構造体
 */
struct SUBSET {
    std::string MtrlName;      ///< マテリアル名
    unsigned int IndexNum = 0; ///< インデックス数
    unsigned int VertexNum = 0;///< 頂点数
    unsigned int IndexBase = 0;///< 開始インデックス
    unsigned int VertexBase = 0;///< 頂点ベース
    unsigned int MaterialIdx = 0;///< マテリアルインデックス
};

/**
 * @enum EBlendState
 * @brief ブレンドステートの種類
 */
enum EBlendState {
    BS_NONE = 0,      ///< 半透明合成無し
    BS_ALPHABLEND,    ///< 半透明合成
    BS_ADDITIVE,      ///< 加算合成
    BS_SUBTRACTION,   ///< 減算合成
    MAX_BLENDSTATE    ///< ブレンドステートの最大値
};

/**
 * @struct CBBoneCombMatrix
 * @brief ボーンコンビネーション行列を保持する構造体
 * @date 20240713
 */
constexpr int MAX_BONE = 400;
struct CBBoneCombMatrix {
    DirectX::XMFLOAT4X4 BoneCombMtx[MAX_BONE];  ///< ボーンコンビネーション行列の配列
};

/**
 * @class Renderer
 * @brief DirectXレンダリング処理を管理するレンダラクラス
 *
 * このクラスは、Direct3Dデバイス、コンテキスト、スワップチェーンなどの管理と、
 * レンダリング処理の初期化、開始、終了などの機能を提供します。
 */
class Renderer : NonCopyable
{
private:
    static D3D_FEATURE_LEVEL m_FeatureLevel;

    static ComPtr<ID3D11Device> m_Device;
    static ComPtr<ID3D11DeviceContext> m_DeviceContext;
    static ComPtr<IDXGISwapChain> m_SwapChain;
    static ComPtr<ID3D11RenderTargetView> m_RenderTargetView;
    static ComPtr<ID3D11DepthStencilView> m_DepthStencilView;

    static ComPtr<ID3D11Buffer> m_WorldBuffer;
    static ComPtr<ID3D11Buffer> m_ViewBuffer;
    static ComPtr<ID3D11Buffer> m_ProjectionBuffer;
    static ComPtr<ID3D11Buffer> m_MaterialBuffer;
    static ComPtr<ID3D11Buffer> m_LightBuffer;

    static ComPtr<ID3D11DepthStencilState> m_DepthStateEnable;
    static ComPtr<ID3D11DepthStencilState> m_DepthStateDisable;

    static ComPtr<ID3D11BlendState> m_BlendState[MAX_BLENDSTATE];
    static ComPtr<ID3D11BlendState> m_BlendStateATC;

    static LIGHT m_Light;
public:
    static void Init();
    static void Dispose();
    static void Begin();
    static void End();
    static void SetDepthEnable(bool Enable);
    static void SetDepthAllwaysWrite();
    static void SetATCEnable(bool Enable);
    static void SetWorldViewProjection2D();
    static void SetWorldMatrix(Matrix4x4* WorldMatrix);
    static void SetViewMatrix(Matrix4x4* ViewMatrix);
    static void SetProjectionMatrix(Matrix4x4* ProjectionMatrix);
    static void SetMaterial(MATERIAL Material);
    static void SetLight(LIGHT Light);
    static ID3D11Device* GetDevice(void) { return m_Device.Get(); }
    static ID3D11DeviceContext* GetDeviceContext(void) { return m_DeviceContext.Get(); }
    static void SetBlendState(int nBlendState);
    static IDXGISwapChain* GetSwapChain() { return m_SwapChain.Get(); }
    static void ClearDepthBuffer() {
        m_DeviceContext->ClearDepthStencilView(m_DepthStencilView.Get(), D3D11_CLEAR_DEPTH, 1.0f, 0);
    }
    static void DisableCulling(bool cullflag = false);
    static void SetFillMode(D3D11_FILL_MODE FillMode);

    static LIGHT GetLight();
};