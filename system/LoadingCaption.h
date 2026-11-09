#pragma once

#include <cmath>

#include "imgui/imgui.h"

/**
 * @brief 画面の右下に「NOW LOADING」を出す。タイトル・リザルトの暗転と飛来の場面で同じ見た目にする。
 * @param timeSeconds 点の数を変えるための時刻。読み込みで止まるフレームでは点の数も止まったままになる。
 * @param alpha 濃さ(0〜255)。
 */
inline void DrawNowLoadingCaption(float timeSeconds, int alpha)
{
	ImGuiViewport* viewport = ImGui::GetMainViewport();
	ImDrawList* drawList = ImGui::GetForegroundDrawList(viewport);
	ImFont* font = ImGui::GetFont();
	constexpr float FONT_SIZE = 24.0f;
	// 点の数が変わっても文字が左右へ動かないよう、3つ分の幅で右端をそろえる。
	const ImVec2 fullSize = font->CalcTextSizeA(FONT_SIZE, 1000.0f, 0.0f, "NOW LOADING...");
	const ImVec2 position(
		viewport->WorkPos.x + viewport->WorkSize.x - fullSize.x - 36.0f,
		viewport->WorkPos.y + viewport->WorkSize.y - fullSize.y - 26.0f);
	const int dots = static_cast<int>(std::fmod(timeSeconds * 2.5f, 4.0f));
	const char* text = dots == 0 ? "NOW LOADING" : dots == 1 ? "NOW LOADING." : dots == 2 ? "NOW LOADING.." : "NOW LOADING...";
	drawList->AddText(font, FONT_SIZE, ImVec2(position.x + 1.5f, position.y + 2.0f), IM_COL32(0, 0, 0, alpha * 3 / 4), text);
	// タイトル画面の作品名と同じ金色。
	drawList->AddText(font, FONT_SIZE, position, IM_COL32(224, 190, 128, alpha), text);
}
