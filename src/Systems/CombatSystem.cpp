#include "CombatSystem.hpp"

#include <IO/Events/UnitAttacked.hpp>
#include <IO/Events/UnitDied.hpp>

#include <random>


namespace sw::systems {

static constexpr uint32_t CloseCombatRange = 1U;
static constexpr uint32_t MinUnitDistance = 1U;
static constexpr uint32_t MaxUnitDistance = 2U;

static size_t getRandomIndex(size_t size) {
    if (size == 0 || size == 1)
        return 0;

    // Optimize?
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<size_t> distr(0, size - 1);
    return distr(gen);
}

static bool isCloseForCombat(const Position& attackerPos, const Position& unitPos, std::pair<uint32_t, uint32_t> range) {
    uint32_t dx = absolute_diff(unitPos.x, attackerPos.x);
    uint32_t dy = absolute_diff(unitPos.y, attackerPos.y);
    uint32_t distance = std::max(dx, dy);

    return (distance >= range.first && distance <= range.second);
}

//
// Combat System
//
CombatSystem::CombatSystem(flecs::world& world, EventLog& eventLog) : BaseSystem(world, eventLog) {
}

void CombatSystem::init() {
    // Register 'Close Combat System'
    _world.system<const Position, const Health, const Strength, const CreationOrder>("CloseCombatSystem")
        .kind(flecs::OnUpdate)
        .order_by<CreationOrder>(OrderComparator)
        .write<HasAttacked>()
        .each([this](flecs::iter& it, size_t i, const Position& pos, const Health& health, const Strength& strength, const CreationOrder& co) {
            _tick = it.world().get_info()->frame_count_total;
            flecs::entity attacker = it.entity(i);
            processCloseCombat(attacker, pos, health, strength);
        });

    // Register 'Distance Combat System'
    _world.system<const Position, const Health, const Agility, const CreationOrder>("AgilityCombatSystem")
        .kind(flecs::OnUpdate)
        .order_by<CreationOrder>(OrderComparator)
        .without<HasAttacked>()
        .write<HasAttacked>()
        .each([this](flecs::iter& it, size_t i, const Position& pos, const Health& health, const Agility& agility, const CreationOrder& co) {
            _tick = it.world().get_info()->frame_count_total;
            flecs::entity attacker = it.entity(i);
            processDistanceCombat(attacker, pos, health, agility);
        });

    // PostUpdate
    _world.system<const Position, const Health, const Strength>("CloseCombatSystemProceed")
        .kind(flecs::PostUpdate)
        .each([this](flecs::iter& it, size_t i, const Position& pos, const Health& health, const Strength& strength) {
            flecs::entity attacker = it.entity(i);
            auto query = _world.query<const Position, const Health, const CloseCombatVulnerable>();
            query.run([&](flecs::iter& it) {
                while (it.next()) {
                    auto p = it.field<const Position>(0);
                    auto h = it.field<const Health>(1);

                    for (auto i : it) {
                        if (attacker.id() == it.entity(i).id())
                            break;
                        if (h[i].points == 0)
                            break;

                        if (isCloseForCombat(pos, p[i], std::make_pair(MinUnitDistance, MinUnitDistance))) {
                            _canProceed = true;
                            break;
                        }
                    }
                    // Manually terminate the iterator before breaking/returning
                    if (_canProceed) {
                        it.fini();
                        break;
                    }
                }
            });
        });

    // PostUpdate
    _world.system<const Position, const Health, const Agility>("AgilityCombatSystemProceed")
        .kind(flecs::PostUpdate)
        .each([this](flecs::iter& it, size_t i, const Position& pos, const Health& health, const Agility& agility) {
            flecs::entity attacker = it.entity(i);
            auto query = _world.query<const Position, const Health, const DistanceCombatVulnerable>();
            query.run([&](flecs::iter& it) {
                while (it.next()) {
                    auto p = it.field<const Position>(0);
                    auto h = it.field<const Health>(1);

                    for (auto i : it) {
                        if (attacker.id() == it.entity(i).id())
                            break;
                        if (h[i].points == 0)
                            break;

                        uint32_t unitDistance = MaxUnitDistance;
                        uint32_t range = agility.range;
                        if (auto debuff = it.entity(i).try_get<AgilityRangeSuppression>()) {
                            unitDistance = (unitDistance > debuff->value ? unitDistance - debuff->value : 1);
                            range = (range > debuff->value ? range - debuff->value : 1);
                        }

                        if (isCloseForCombat(pos, p[i], std::make_pair(unitDistance, range))) {
                            _canProceed = true;
                            break;
                        }
                    }
                    // Manually terminate the iterator before breaking/returning
                    if (_canProceed) {
                        it.fini();
                        break;
                    }
                }
            });
        });
}

bool CombatSystem::canProceed() const {
    return _canProceed;
}

void CombatSystem::preUpdate() {
    _canProceed = false;
}

void CombatSystem::postUpdate() {
}

void CombatSystem::processCloseCombat(flecs::entity attacker, const Position& pos, const Health& health, const Strength& strength) {
    // 'Dead' units are flushed after combat complete
    if (!attacker.is_alive() || health.points == 0)
        return;

    std::vector<flecs::entity_t> units;
    // 1. Find suitable units to attack
    auto query = _world.query<const Position, const Health, const CloseCombatVulnerable>();
    query.run([&](flecs::iter& it) {
        while (it.next()) {
            auto p = it.field<const Position>(0);
            auto h = it.field<const Health>(1);

            for (auto i : it) {
                if (attacker.id() == it.entity(i).id())
                    break;
                if (h[i].points == 0)
                    break;

                if (isCloseForCombat(pos, p[i], std::make_pair(MinUnitDistance, MinUnitDistance))) {
                    units.push_back(it.entity(i).id());
                }
            }
        }
    });

    if (units.empty())
        return;

    size_t idx = getRandomIndex(units.size());
    // 2. Attack random unit
    flecs::entity targetUnit = _world.entity(units[idx]);
    attackUnit(attacker, targetUnit, strength.value);
}

void CombatSystem::processDistanceCombat(flecs::entity attacker, const Position& pos, const Health& health, const Agility& agility) {
    // 'Dead' units are flushed after combat complete
    if (!attacker.is_alive() || health.points == 0)
        return;

    std::vector<flecs::entity_t> units; 
    // 1. Find suitable units to attack
    auto query = _world.query<const Position, const Health, const DistanceCombatVulnerable>();
    query.run([&](flecs::iter& it) {
        while (it.next()) {
            auto p = it.field<const Position>(0);
            auto h = it.field<const Health>(1);

            for (auto i : it) {
                if (attacker.id() == it.entity(i).id()) {
                    break;
                }
                if (h[i].points == 0) {
                    break;
                }

                uint32_t unitDistance = MaxUnitDistance;
                uint32_t range = agility.range;
                if (auto debuff = it.entity(i).try_get<AgilityRangeSuppression>()) {
                    unitDistance = (unitDistance > debuff->value ? unitDistance - debuff->value : 1);
                    range = (range > debuff->value ? range - debuff->value : 1);
                }

                if (isCloseForCombat(pos, p[i], std::make_pair(unitDistance, range))) {
                    units.push_back(it.entity(i).id());
                }
            }
        }
    });

    if (units.empty())
        return;

    size_t idx = getRandomIndex(units.size());
    // 2. Attack random unit
    flecs::entity targetUnit = _world.entity(units[idx]);
    attackUnit(attacker, targetUnit, agility.value);
}

void CombatSystem::attackUnit(flecs::entity attacker, flecs::entity& targetUnit, uint32_t strength) {
    if (!targetUnit.is_alive())
        return;

    // Can't attack unit without health component
    Health* health = targetUnit.try_get_mut<Health>();
    if (nullptr == health)
        return;

    const UnitId& unitId = targetUnit.get<UnitId>();
    if (strength >= health->points) {
        health->points = 0;
        targetUnit.destruct();
    }
    else {
        health->points -= strength;
    }

    // Attack event first
    _eventLog.log(_tick, io::UnitAttacked{
        attacker.get<UnitId>().value,
        unitId.value,
        strength,
        health->points
    });

    // Death event than
    if (health->points == 0) {
        _eventLog.log(_tick, io::UnitDied{unitId.value});
    }

    attacker.add<HasAttacked>();
}

} // namespace sw::systems
