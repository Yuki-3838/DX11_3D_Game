#include "GameApp.h"

#include <Windows.h>

/**
 * @brief Windowsアプリケーションの開始点。
 *
 * Phase 1ではここでGameAppを作り、初期化に成功した場合だけメインループへ入ります。
 */
int APIENTRY wWinMain(HINSTANCE hInstance, HINSTANCE, LPWSTR, int)
{
    GameApp app;
    if (!app.Init(hInstance, 1280, 720))
    {
        MessageBox(nullptr, L"GameApp initialization failed.", L"DX11_3D_Game", MB_OK | MB_ICONERROR);
        return -1;
    }

    app.Run();
    return 0;
}
