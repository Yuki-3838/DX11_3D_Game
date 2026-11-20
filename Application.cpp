//-----------------------------------------------------------------------------
// ヘッダーファイルを読み込む。
//-----------------------------------------------------------------------------
#include <Windows.h>
#include <cstdint>
#include "Application.h"
#include "game.h"
#include "system/imgui/imgui.h"

//-----------------------------------------------------------------------------
// アプリケーション全体で使う定数。
//-----------------------------------------------------------------------------
constexpr auto ClassName = TEXT("就職作品");        //!< ウィンドウクラス名.
constexpr auto WindowName = TEXT("就職作品");        //!< ウィンドウ名.

//-----------------------------------------------------------------------------
// クラスで共有する静的メンバー。
//-----------------------------------------------------------------------------
HINSTANCE  Application::m_hInst;        //!< インスタンスハンドルです.
HWND       Application::m_hWnd;         //!< ウィンドウハンドルです.
uint32_t   Application::m_Width;        //!< ウィンドウの横幅です.
uint32_t   Application::m_Height;       //!< ウィンドウの縦幅です.

// ImGuiのWin32プロシージャハンドラ(マウス対応)
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

///////////////////////////////////////////////////////////////////////////////
// アプリケーションクラスの実装。
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
    // Windows の表示の拡大(125% など)で、ウィンドウを勝手に引き伸ばされないようにする。
    // これがないと、画面の大きさも拡大後の小さな値で見えてしまい、ぼやけた表示になる。
    SetProcessDPIAware();

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

    // ウィンドウの置き場所と形を決める.
    // 枠付きのウィンドウが画面(タスクバーを除いた範囲)に収まるなら、枠付きで画面の真ん中に置く。
    // 収まらないとき(1920x1080 の画面で 1920x1080 のゲームを出すと、タイトルバーの分だけ下が切れる)は、
    // 枠なしで画面全体を覆う。画面の大きさとゲームの大きさが同じなら、ぴったり全部が見える。
    // 画面のほうが小さいときは、描画は main.h の大きさのまま行い、表示のときに画面へ縮めて出す。
    const POINT origin{ 0, 0 };
    MONITORINFO monitor{};
    monitor.cbSize = sizeof(monitor);
    GetMonitorInfo(MonitorFromPoint(origin, MONITOR_DEFAULTTOPRIMARY), &monitor);
    const RECT& screen = monitor.rcMonitor;
    const RECT& work = monitor.rcWork;

    DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
    RECT rc = {};
    rc.right = static_cast<LONG>(m_Width);
    rc.bottom = static_cast<LONG>(m_Height);
    AdjustWindowRect(&rc, style, FALSE);
    int windowWidth = rc.right - rc.left;
    int windowHeight = rc.bottom - rc.top;
    int windowX = 0;
    int windowY = 0;
    const int workWidth = work.right - work.left;
    const int workHeight = work.bottom - work.top;
    if (windowWidth <= workWidth && windowHeight <= workHeight)
    {
        windowX = work.left + (workWidth - windowWidth) / 2;
        windowY = work.top + (workHeight - windowHeight) / 2;
    }
    else
    {
        style = WS_POPUP | WS_SYSMENU | WS_MINIMIZEBOX;
        windowX = screen.left;
        windowY = screen.top;
        windowWidth = screen.right - screen.left;
        windowHeight = screen.bottom - screen.top;
    }

    // ウィンドウを生成.
    m_hWnd = CreateWindowEx(
        0,
        ClassName,
        WindowName,
        style,
        windowX,
        windowY,
        windowWidth,
        windowHeight,
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

    case WM_SYSCOMMAND:
    {
        // Altを単独で押すと、Windowsはウィンドウのメニューを開こうとしてキー入力を奪い、
        // その後の移動キーなどが効かなくなる。ゲームではAltをカーソルの出し入れに使うので止める。
        if ((wp & 0xFFF0) == SC_KEYMENU)
            return 0;
    }
    break;

    default:
    { /* DO_NOTHING */ }
    break;
    }

    return DefWindowProc(hWnd, msg, wp, lp);
}
