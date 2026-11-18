#pragma once

#include <cctype>
#include <cmath>
#include <cstring>

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

/**
 * @brief 読み込みが終わったあと、NOW LOADING と同じ場所へ「PRESS ENTER」(パッドなら PRESS A)を出す。
 * 押すのを待っていることが分かるよう、ゆっくり明滅させる。
 * @param buttonLabel 押すボタンの名前(GameInput::GetButtonLabel)。大文字にして出す。
 * @param timeSeconds 明滅に使う時刻。
 * @param alpha 濃さ(0〜255)。
 */
inline void DrawPressButtonCaption(const char* buttonLabel, float timeSeconds, int alpha)
{
	char text[64] = "PRESS ";
	size_t length = std::strlen(text);
	for (const char* c = buttonLabel; *c != '\0' && length + 1 < sizeof(text); ++c)
		text[length++] = static_cast<char>(std::toupper(static_cast<unsigned char>(*c)));
	text[length] = '\0';

	ImGuiViewport* viewport = ImGui::GetMainViewport();
	ImDrawList* drawList = ImGui::GetForegroundDrawList(viewport);
	ImFont* font = ImGui::GetFont();
	constexpr float FONT_SIZE = 24.0f;
	const ImVec2 size = font->CalcTextSizeA(FONT_SIZE, 1000.0f, 0.0f, text);
	const ImVec2 position(
		viewport->WorkPos.x + viewport->WorkSize.x - size.x - 36.0f,
		viewport->WorkPos.y + viewport->WorkSize.y - size.y - 26.0f);
	const float pulse = 0.55f + 0.45f * (0.5f + 0.5f * std::cos(timeSeconds * 3.2f));
	const int shown = static_cast<int>(static_cast<float>(alpha) * pulse);
	drawList->AddText(font, FONT_SIZE, ImVec2(position.x + 1.5f, position.y + 2.0f), IM_COL32(0, 0, 0, shown * 3 / 4), text);
	drawList->AddText(font, FONT_SIZE, position, IM_COL32(224, 190, 128, shown), text);
}
