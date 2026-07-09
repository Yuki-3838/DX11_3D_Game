//-----------------------------------------------------------------------------
// Includes
//-----------------------------------------------------------------------------
#include <Windows.h>
#include <cstdint>
#include "Application.h"
#include "game.h"
#include "system/imgui/imgui.h"

//-----------------------------------------------------------------------------
// Constant Values.
//-----------------------------------------------------------------------------
constexpr auto ClassName = TEXT("2026 GM31 ひな形");        //!< ウィンドウクラス名.
constexpr auto WindowName = TEXT("2026 GM31 ひな形");        //!< ウィンドウ名.

//-----------------------------------------------------------------------------
// Class Static
//-----------------------------------------------------------------------------
HINSTANCE  Application::m_hInst;        //!< インスタンスハンドルです.
HWND       Application::m_hWnd;         //!< ウィンドウハンドルです.
uint32_t   Application::m_Width;        //!< ウィンドウの横幅です.
uint32_t   Application::m_Height;       //!< ウィンドウの縦幅です.

// ImGuiのWin32プロシージャハンドラ(マウス対応)
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

///////////////////////////////////////////////////////////////////////////////
// Application class
///////////////////////////////////////////////////////////////////////////////

/**
 * @brief コンストラクタです.
 * @param width ウィンドウの横幅です.
 * @param height ウィンドウの縦幅です.
 * @details ウィンドウのサイズを初期化し、タイマー精度を設定します。
 */
Application::Application(uint32_t width, uint32_t height)
{
    m_Height = height;
    m_Width = width;

    timeBeginPeriod(1);
}

/**
 * @brief デストラクタです.
 * @details タイマー精度を元に戻します。
 */
Application::~Application()
{
    timeEndPeriod(1);
}

/**
 * @brief アプリケーションを実行します.
 * @details 初期化に成功した場合、メインループを開始します。
 */
void Application::Run()
{
    if (InitApp())
    {
        MainLoop();
    }

    TermApp();
}

/**
 * @brief アプリケーションの初期化を行います.
 * @return 初期化が成功した場合にtrueを返します。
 * @details ウィンドウの初期化が含まれます。
 */
bool Application::InitApp()
{
    // ウィンドウの初期化.
    if (!InitWnd())
    {
        return false;
    }

    // 正常終了.
    return true;
}

/**
 * @brief アプリケーションの終了処理を行います.
 * @details ウィンドウの終了処理が含まれます。
 */
void Application::TermApp()
{
    // ウィンドウの終了処理.
    TermWnd();
}

/**
 * @brief ウィンドウを初期化します.
 * @return 初期化に成功した場合にtrueを返します。
 * @details ウィンドウクラスの登録とウィンドウの生成を行います。
 */
bool Application::InitWnd()
{
    // インスタンスハンドルを取得.
    auto hInst = GetModuleHandle(nullptr);
    if (hInst == nullptr)
    {
        return false;
    }

    // ウィンドウの設定.
    WNDCLASSEX wc = {};
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.hIcon = LoadIcon(hInst, IDI_APPLICATION);
    wc.hCursor = LoadCursor(hInst, IDC_ARROW);
    wc.hbrBackground = GetSysColorBrush(COLOR_BACKGROUND);
    wc.lpszMenuName = nullptr;
    wc.lpszClassName = ClassName;
    wc.hIconSm = LoadIcon(hInst, IDI_APPLICATION);

    // ウィンドウの登録.
    if (!RegisterClassEx(&wc))
    {
        return false;
    }

    // インスタンスハンドル設定.
    m_hInst = hInst;

    // ウィンドウのサイズを設定.
    RECT rc = {};
    rc.right = static_cast<LONG>(m_Width);
    rc.bottom = static_cast<LONG>(m_Height);

    // ウィンドウサイズを調整.
    auto style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU;
    AdjustWindowRect(&rc, style, FALSE);

    // ウィンドウを生成.
    m_hWnd = CreateWindowEx(
        0,
        ClassName,
        WindowName,
        style,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        rc.right - rc.left,
        rc.bottom - rc.top,
        nullptr,
        nullptr,
        m_hInst,
        nullptr);

    if (m_hWnd == nullptr)
    {
        return false;
    }

    // ウィンドウを表示.
    ShowWindow(m_hWnd, SW_SHOWNORMAL);

    // ウィンドウを更新.
    UpdateWindow(m_hWnd);

    // ウィンドウにフォーカスを設定.
    SetFocus(m_hWnd);

    // 正常終了.
    return true;
}

/**
 * @brief ウィンドウの終了処理を行います.
 * @details ウィンドウクラスの登録解除を行います。
 */
void Application::TermWnd()
{
    // ウィンドウの登録を解除.
    if (m_hInst != nullptr)
    {
        UnregisterClass(ClassName, m_hInst);
    }

    m_hInst = nullptr;
    m_hWnd = nullptr;
}

/**
 * @brief メインループを実行します.
 * @details メッセージを処理します。WM_QUITメッセージを受け取るまで続きます。
 */
void Application::MainLoop()
{
    MSG msg = {};

    gameinit();

    while (WM_QUIT != msg.message)
    {
        if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE) == TRUE)
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        else {
            // ゲームループ
            gameloop();
        }
    }

    gamedispose();

}

/**
 * @brief ウィンドウプロシージャです.
 * @param hWnd ウィンドウハンドルです.
 * @param msg メッセージです.
 * @param wp WPARAM パラメータです.
 * @param lp LPARAM パラメータです.
 * @return 処理結果を返します.
 * @details ウィンドウのメッセージを処理します。
 */
LRESULT CALLBACK Application::WndProc(HWND hWnd, UINT msg, WPARAM wp, LPARAM lp)
{
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wp, lp))
        return true;

    switch (msg)
    {
    case WM_DESTROY:
    {
        PostQuitMessage(0);
    }
    break;

    default:
    { /* DO_NOTHING */ }
    break;
    }

    return DefWindowProc(hWnd, msg, wp, lp);
}
