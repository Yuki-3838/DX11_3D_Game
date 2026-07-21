#include "../Application.h"

#include <Windows.h>

/**
 * @brief Windowsアプリケーションの開始点。
 *
 * Phase 1ではここでGameAppを作り、初期化に成功した場合だけメインループへ入ります。
 */
int APIENTRY wWinMain(HINSTANCE, HINSTANCE, LPWSTR, int)
{
    Application app(1280, 720);
    app.Run();
    return 0;
}
