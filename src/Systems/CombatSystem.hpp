#pragma once

#include <Components/Components.hpp>

#include <Systems/BaseSystem.hpp>


namespace sw::systems {

using namespace components;

class CombatSystem : public BaseSystem {
public:
    CombatSystem(flecs::world& world, EventLog& eventLog);
    ~CombatSystem() = default;

    void init() override;

    bool canProceed() const override;

    void preUpdate() override;

    void postUpdate() override;

private:
    void processCloseCombat(flecs::entity attacker, const Position& pos, const Health& health, const Strength& strength);

    void processDistanceCombat(flecs::entity attacker, const Position& pos, const Health& health, const Agility& agility);

    void attackUnit(flecs::entity attacker, flecs::entity& targetUnit, uint32_t strength);

private:
    bool _canProceed = false;
};

} // namespace sw::systems
