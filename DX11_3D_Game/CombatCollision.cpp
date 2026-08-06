#include "CombatCollision.h"

#include <algorithm>
#include <cmath>

namespace Combat
{
namespace
{
using namespace GM31::GE;
using namespace GM31::GE::Collision;
constexpr float kPi = 3.14159265358979323846f;

float DotXZ(const Vector3& a, const Vector3& b) { return a.x * b.x + a.z * b.z; }

Vector3 NormalizeXZ(Vector3 value)
{
    const float length = std::sqrt(value.x * value.x + value.z * value.z);
    if (length <= 0.0001f) return Vector3(1.0f, 0.0f, 0.0f);
    value.x /= length;
    value.z /= length;
    value.y = 0.0f;
    return value;
}

BoundingCapsule MakeHurtbox(const CombatantCollisionState& target)
{
    const float halfHeight = std::max(0.0f, target.hurtbox.height * 0.5f - target.hurtbox.radius);
    const Vector3 center = target.position + Vector3(0.0f, target.hurtbox.height * 0.5f, 0.0f);
    return {center - Vector3(0.0f, halfHeight, 0.0f), center + Vector3(0.0f, halfHeight, 0.0f), target.hurtbox.radius};
}

BoundingCapsule MakeCapsuleHitbox(const HitboxDefinition& hitbox, const CombatantCollisionState& attacker)
{
    const Vector3 forward = NormalizeXZ(attacker.forward);
    const Vector3 start = attacker.position + Vector3(0.0f, 0.9f, 0.0f) + forward * 0.35f;
    const Vector3 end = start + forward * std::max(0.0f, hitbox.length);
    return {start, end, std::max(0.0f, hitbox.radius)};
}
}

AttackCollisionResult EvaluateAttackCollision(const AttackData& attack, const CombatantCollisionState& attacker, const std::vector<CombatantCollisionState>& targets)
{
    AttackCollisionResult result;
    const Vector3 forward = NormalizeXZ(attacker.forward);
    const float maxDistance = std::max(0.0f, attack.broadPhaseFilter.maxDistance);
    result.debugVolumes.push_back({CollisionDebugVolume::Type::BroadPhaseSphere, attacker.position, {}, maxDistance, false});

    for (const CombatantCollisionState& target : targets)
    {
        if (target.id == attacker.id || (target.downed && !attack.broadPhaseFilter.acceptsDownedTarget)) continue;
        const Vector3 toTarget = target.position - attacker.position;
        const float distance = std::sqrt(DotXZ(toTarget, toTarget));
        const Vector3 direction = NormalizeXZ(toTarget);
        const float angle = std::acos(std::clamp(DotXZ(forward, direction), -1.0f, 1.0f)) * 180.0f / kPi;
        if (distance > maxDistance || angle > attack.broadPhaseFilter.maxAngleDegrees) continue;

        ++result.broadPhaseCandidateCount;
        const BoundingCapsule targetCapsule = MakeHurtbox(target);
        bool hit = false;
        for (const HitboxDefinition& hitbox : attack.hitboxes)
        {
            const BoundingCapsule hitCapsule = MakeCapsuleHitbox(hitbox, attacker);
            if (hitbox.narrowPhase == NarrowPhaseType::Sphere)
            {
                const Vector3 center = hitCapsule.startpoint + forward * (hitbox.length * 0.5f);
                Collision::Segment segment{targetCapsule.startpoint, targetCapsule.endpoint};
                Vector3 closest{};
                float t = 0.0f;
                hit = calcPointSegmentDist(center, segment, closest, t) <= hitbox.radius + targetCapsule.radius;
            }
            else
            {
                hit = CollisionCapsule(hitCapsule, targetCapsule);
            }
            if (hit)
            {
                result.pairs.push_back({attacker.id, target.id, attack.attackId, hitbox.name});
                result.debugVolumes.push_back({CollisionDebugVolume::Type::AttackCapsule, hitCapsule.startpoint, hitCapsule.endpoint, hitCapsule.radius, true});
                break;
            }
        }
        result.debugVolumes.push_back({CollisionDebugVolume::Type::TargetHurtbox, targetCapsule.startpoint, targetCapsule.endpoint, targetCapsule.radius, hit});
    }
    return result;
}
}
