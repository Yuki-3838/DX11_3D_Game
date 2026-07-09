#pragma once

#pragma comment(lib, "winmm.lib")

#include <Windows.h>
#include <cstdint>

/**
 * @brief アプリケーションクラスです.
 * @details このクラスはアプリケーションの初期化、終了処理、メインループを担当します。
 * 非コピーアブルなクラスとして設計 ウィンドウハンドルやインスタンスハンドルを管理します。
 */
class Application
{
public:
    /**
     * @brief デフォルトコンストラクタ。
     */
    Application() = default;

    /**
     * @brief コピーコンストラクタを削除。
     * @details このクラスのオブジェクトのコピーは許可されていないため、コピーコンストラクタは削除されています。
     */
    Application(const Application&) = delete;

    /**
     * @brief コピー代入演算子を削除。
     * @details このクラスのオブジェクトの代入は許可されていないため、コピー代入演算子は削除されています。
     */
    Application& operator=(const Application&) = delete;
    /**
     * @brief コンストラクタです.
     * @param width ウィンドウの横幅です.
     * @param height ウィンドウの縦幅です.
     * @details ウィンドウのサイズを設定し、アプリケーションを初期化します。
     */
    Application(uint32_t width, uint32_t height);

    /**
     * @brief デストラクタです.
     * @details アプリケーションの終了処理を行います。
     */
    ~Application();

    /**
     * @brief アプリケーションを実行します.
     * @details メインループを開始し、アプリケーションのメイン処理を実行します。
     */
    void Run();

    /**
     * @brief ウィンドウの横幅を取得します.
     * @return ウィンドウの横幅を返します.
     */
    static uint32_t GetWidth() {
        return m_Width;
    }
    
    /**
     * @brief ウィンドウの縦幅を取得します.
     * @return ウィンドウの縦幅を返します.
     */
    static uint32_t GetHeight() {
        return m_Height;
    }

    /**
     * @brief ウィンドウハンドルを取得します.
     * @return ウィンドウハンドルを返します.
     */
    static HWND GetWindow() {
        return m_hWnd;
    }

    /**
     * @brief インスタンスハンドルを取得します.
     * @return インスタンスハンドルを返します.
     */
    static HINSTANCE GetHInstance() {
        return m_hInst;
    }

private:
    static HINSTANCE   m_hInst;    ///< インスタンスハンドルです.
    static HWND        m_hWnd;     ///< ウィンドウハンドルです.
    static uint32_t    m_Width;    ///< ウィンドウの横幅です.
    static uint32_t    m_Height;   ///< ウィンドウの縦幅です.

    static bool InitApp();
    static void TermApp();
    static bool InitWnd();
    static void TermWnd();
    static void MainLoop();

    /**
     * @brief ウィンドウプロシージャ関数です.
     * @param hWnd ウィンドウハンドルです.
     * @param msg メッセージです.
     * @param wp WPARAM パラメータです.
     * @param lp LPARAM パラメータです.
     * @return 処理結果を返します.
     * @details ウィンドウに送られたメッセージを処理します。
     */
    static LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wp, LPARAM lp);
};
