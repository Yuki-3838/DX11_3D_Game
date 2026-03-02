#pragma once
#include <Windows.h>
#include <d3d11.h>
#include <wrl/client.h>//ComPtrを使用するためのヘッダ

class GameApp
{
private:
	//DirectX関連
	Microsoft::WRL::ComPtr<ID3D11Device> m_device;                    //リソース(バッファ等)生成用
	Microsoft::WRL::ComPtr<ID3D11DeviceContext> m_deviceContext;      //描画コマンド発行用
	Microsoft::WRL::ComPtr<IDXGISwapChain> m_swapChain;               //画面フリップ(表裏切り替え)用
	Microsoft::WRL::ComPtr<ID3D11RenderTargetView> m_renderTargetView;//描画先(バックバッファ)

	//内部処理関数
	bool InitWindow(HINSTANCE hInstance);
	bool InitDirectX();

	void Update();
	void Draw();
	void Cleanup();

	//ウィンドプロシージャ(Windowsのシステムメッセージを受け取る)
	static LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
public:
	GameApp();
	~GameApp();

	//アプリケーションの初期化
	void Init(HINSTANCE hInstance,int windowWidth,int windowHeight);
	
	//アプリケーションのメインループ
	void Run();
};
