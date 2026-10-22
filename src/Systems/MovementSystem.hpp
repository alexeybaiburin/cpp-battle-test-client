#pragma once

#include <Systems/BaseSystem.hpp>

#include <Components/Components.hpp>


namespace sw::systems {

using namespace components;

class MovementSystem : public BaseSystem {
public:
    MovementSystem(flecs::world& world, EventLog& eventLog);
    ~MovementSystem() = default;

    void init() override;

    bool canProceed() const override;

private:
    void processUnitMovement(flecs::entity unit, Position& pos, const Health& health, const MarchDest& marchDest);
};

} // namespace sw::systems
