#include "../main.h"
#include "../Application.h"

#include <Windows.h>

/**
 * @brief Windowsアプリケーションの開始点。
 *
 * Phase 1ではここでGameAppを作り、初期化に成功した場合だけメインループへ入ります。
 * ウィンドウの大きさは main.h の SCREEN_WIDTH / SCREEN_HEIGHT で決める
 * (リポジトリ直下の main.cpp はビルドから外れているので、ここが実際の開始点)。
 */
int APIENTRY wWinMain(HINSTANCE, HINSTANCE, LPWSTR, int)
{
    Application app(SCREEN_WIDTH, SCREEN_HEIGHT);
    app.Run();
    return 0;
}
