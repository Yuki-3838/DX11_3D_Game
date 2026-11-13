#pragma once

#include <chrono>
#include <iostream>

/**
 * 起動にかかる時間の記録。起動してからの経過ミリ秒を [Startup] 付きで出力する(コンソール・撮影ログ用)。
 * 起動直後の黒い画面が長いとき、どこに時間がかかっているかを見るために使う。
 */
inline void LogStartup(const char* label)
{
    static const auto start = std::chrono::steady_clock::now();
    const auto now = std::chrono::steady_clock::now();
    const long long ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - start).count();
    std::cout << "[Startup] " << ms << "ms " << label << std::endl;
}
