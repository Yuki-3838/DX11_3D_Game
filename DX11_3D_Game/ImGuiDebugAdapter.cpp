#include "ImGuiDebugAdapter.h"

#include "CombatDesign.h"
#include "DebugOverlay.h"

#if defined(DX11_GAME_ENABLE_IMGUI)
#include "../system/imgui/imgui.h"
#endif

void ImGuiDebugAdapter::Render(const DebugOverlay& overlay)
{
#if defined(DX11_GAME_ENABLE_IMGUI)
    const PerformanceSnapshot& performance = overlay.GetPerformanceSnapshot();
    const Combat::CombatDebugState& combat = overlay.GetCombatDebugState();

    ImGui::Begin("DX11 Debug");
    ImGui::Text("Frame %.3f ms", performance.frameMilliseconds);
    ImGui::Text("FPS %.1f", performance.estimatedFps);
    ImGui::Text("Fixed Updates %d", performance.fixedUpdateCount);
    ImGui::Separator();
    ImGui::Text("CPU Budget %.2f ms", performance.budget.cpuMilliseconds);
    ImGui::Text("GPU Budget %.2f ms", performance.budget.gpuMilliseconds);
    ImGui::Text("Systems Budget %.2f ms", performance.budget.systemsMilliseconds);
    ImGui::Separator();
    ImGui::Text("Attack %s", combat.currentAttackId.c_str());
    ImGui::Text("Phase %s", Combat::ToString(combat.currentPhase));
    ImGui::Text("Player HP %d", combat.playerHp);
    ImGui::Text("Player Stamina %d", combat.playerStamina);
    ImGui::Text("Enemy HP %d", combat.enemyHp);
    ImGui::Text("Distance %.2f m", combat.distanceMeters);
    ImGui::Text("Guarding %s", combat.playerGuarding ? "true" : "false");
    ImGui::Text("Enemy Recovery %s", combat.enemyInRecovery ? "true" : "false");
    ImGui::Text("Broad Candidates %d", combat.broadPhaseCandidateCount);
    ImGui::Text("Collisions %d", combat.confirmedCollisionCount);
    ImGui::End();
#else
    (void)overlay;
#endif
}
