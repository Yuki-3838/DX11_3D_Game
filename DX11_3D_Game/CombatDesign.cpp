#include "CombatDesign.h"

namespace Combat
{
AttackData CreatePrototypeHeavySlash()
{
    AttackData attack;
    attack.attackId = "enemy_heavy_slash";
    attack.animationName = "Enemy_HeavySlash_A";
    attack.frames.anticipationSeconds = 0.70f;
    attack.frames.activeSeconds = 0.18f;
    attack.frames.recoverySeconds = 0.90f;
    attack.frames.cooldownSeconds = 0.35f;
    attack.frames.commitSeconds = 0.55f;
    attack.broadPhaseFilter.maxDistance = 3.4f;
    attack.broadPhaseFilter.maxAngleDegrees = 65.0f;
    attack.hitboxes.push_back({"weapon_blade", NarrowPhaseType::Capsule, 0.22f, 1.4f});
    attack.guardType = GuardType::GuardBreak;
    attack.damage = 28;
    attack.staminaDamage = 30;
    attack.postureDamage = 20;
    attack.trackingAngleDegrees = 35.0f;
    attack.trackingSpeed = 2.2f;
    attack.moveDistance = 1.2f;
    attack.turnLimitDegrees = 40.0f;
    attack.vfx = "vfx_heavy_slash_arc";
    attack.sfx = "sfx_heavy_slash";
    attack.nextActionCandidates = {"enemy_recover_step", "enemy_guard", "enemy_short_slash"};
    attack.fatigueCost = 12.0f;
    return attack;
}

const char* ToString(AttackPhase phase)
{
    switch (phase)
    {
    case AttackPhase::Idle:
        return "Idle";
    case AttackPhase::Anticipation:
        return "Anticipation";
    case AttackPhase::Active:
        return "Active";
    case AttackPhase::Recovery:
        return "Recovery";
    case AttackPhase::Cooldown:
        return "Cooldown";
    default:
        return "Unknown";
    }
}
} // namespace Combat
