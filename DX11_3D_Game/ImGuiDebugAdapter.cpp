#include "ImGuiDebugAdapter.h"

#include "CombatDesign.h"
#include "DebugOverlay.h"

#if defined(DX11_GAME_ENABLE_IMGUI)
#include "../system/imgui/imgui.h"
#endif

void ImGuiDebugAdapter::Render(DebugOverlay& overlay)
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
    ImGui::Text("Enemy Recovery %s", combat.enemyInRecovery ? "true" : "false");
    ImGui::Text("Broad Candidates %d", combat.broadPhaseCandidateCount);
    ImGui::Text("Collisions %d", combat.confirmedCollisionCount);
    ImGui::Text("Narrow Hit %s", combat.narrowPhaseHit ? "true" : "false");
    ImGui::Text("Broad Radius %.2f", combat.broadPhaseRadius);
    ImGui::Text("Hitbox Length %.2f", combat.attackHitboxLength);
    ImGui::End();

    ImGui::Begin("Attack Collision Debug");    ImGui::Checkbox("Draw collision volumes", &overlay.MutableCombatDebugState().debugDrawCollision);
    ImGui::Separator();
    ImGui::Text("Phase: %s", Combat::ToString(combat.currentPhase));
    ImGui::Text("Hitbox active: %s", combat.attackHitboxActive ? "true" : "false");
    ImGui::Text("Broad phase candidates: %d", combat.broadPhaseCandidateCount);
    ImGui::Text("Narrow phase hit: %s", combat.narrowPhaseHit ? "true" : "false");
    ImGui::Text("Confirmed hits: %d", combat.confirmedCollisionCount);
    ImGui::Text("Broad phase radius: %.2f", combat.broadPhaseRadius);
    ImGui::Text("Attack hitbox length: %.2f", combat.attackHitboxLength);
    ImGui::Separator();
    ImGui::TextColored(ImVec4(1.0f, 0.25f, 0.08f, 1.0f), "RED: attack hitbox");
    ImGui::TextColored(ImVec4(0.30f, 0.90f, 0.35f, 1.0f), "GREEN: target hurtbox");
    ImGui::TextColored(ImVec4(0.35f, 0.24f, 0.12f, 1.0f), "BROWN: broad phase range");
    ImGui::Text("Space: restart attack");
    ImGui::End();
#else
    (void)overlay;
#endif
}
