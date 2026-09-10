#pragma once

#include "CombatDesign.h"
#include "../system/collision.h"

#include <vector>

namespace Combat
{
struct CombatantCollisionState
{
    std::string id;
    ::Vector3 position{};
    ::Vector3 forward{1.0f, 0.0f, 0.0f};
    HurtboxDefinition hurtbox;
    bool guarding = false;
    bool downed = false;
};

struct CollisionDebugVolume
{
    enum class Type { BroadPhaseSphere, AttackCapsule, TargetHurtbox };
    Type type = Type::BroadPhaseSphere;
    ::Vector3 a{};
    ::Vector3 b{};
    float radius = 0.0f;
    bool hit = false;
};

struct AttackCollisionResult
{
    int broadPhaseCandidateCount = 0;
    std::vector<CollisionPair> pairs;
    std::vector<CollisionDebugVolume> debugVolumes;
};

/** 軽量な候補絞り込みを行った後、攻撃判定と受け判定を正確に検証する。 */
AttackCollisionResult EvaluateAttackCollision(
    const AttackData& attack,
    const CombatantCollisionState& attacker,
    const std::vector<CombatantCollisionState>& targets);
}
