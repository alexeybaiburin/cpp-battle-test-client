#include "MovementSystem.hpp"

#include <IO/Events/MarchStarted.hpp>
#include <IO/Events/UnitMoved.hpp>
#include <IO/Events/MarchEnded.hpp>


namespace sw::systems {

// Should be part of 'globa' Helper/Utils class
static bool isPositionOccupired(flecs::world& world, uint32_t x, uint32_t y) {
    bool isOccupired(false);
    auto query = world.query<Position>();
    query.each([&](flecs::entity e, Position& p) {
        if (e.has<GridPresence, Ignore>()) {
            return;
        }
        if (x == p.x && y == p.y) {
            isOccupired = true;
            return;
        }
    });
    return isOccupired;
}

//
// Movement System
//
MovementSystem::MovementSystem(flecs::world& world, EventLog& eventLog) : BaseSystem(world, eventLog) {
}

void MovementSystem::init() {
    // Register 'March' movement system
    _world.system<Position, const Health, const MarchDest, const CreationOrder>("MarchUnitSystem")
        .kind(flecs::OnValidate)
        .order_by<CreationOrder>(OrderComparator)
        .without<HasAttacked>()
        .without<Stationary>()    // Check for 'Tower' and 'Mine/Bomb' units
        .each([this](flecs::iter& it, size_t i, Position& pos, const Health& health, const MarchDest& marchDest, const CreationOrder&) {
            _tick = it.world().get_info()->frame_count_total;
            uint32_t marchSteps = 1;
            // Some units can make several steps
            if (auto msComponent = it.entity(i).try_get<MarchSteps>()) {
                marchSteps = msComponent->value;
            }
            for (uint32_t i = 0; i < marchSteps; ++i) {
                processUnitMovement(it.entity(i), pos, health, marchDest);
            }
        });

    _world.system<const Position>("CleanUptSystem")
        .kind(flecs::PostUpdate)
        .with<HasAttacked>()
        .each([](flecs::entity unit, const Position& pos) {
            unit.remove<HasAttacked>();
        });
}

bool MovementSystem::canProceed() const {
    return _world.count<components::MarchDest>() > 0;
}

void MovementSystem::processUnitMovement(flecs::entity unit, Position& pos, const Health& health, const MarchDest& marchDest) {
    // 'Dead' units are flushed after combat complete
    if (!unit.is_alive() || health.points == 0)
        return;

    if (pos.x == marchDest.startX && pos.y == marchDest.startY) {
        _eventLog.log(_tick, io::MarchStarted{unit.get<UnitId>().value, marchDest.startX, marchDest.startY, marchDest.targetX, marchDest.targetY});
    }

    Position current {pos.x, pos.y};
    Position destination {marchDest.targetX, marchDest.targetY};

    if (current != destination) {
        if (destination.x > current.x) ++current.x;
        if (destination.x < current.x) --current.x;

        if (destination.y > current.y) ++current.y;
        if (destination.y < current.y) --current.y;
    }

    if ((current != pos) && !isPositionOccupired(_world, current.x, current.y)) {
        pos = current;
        _eventLog.log(_tick, io::UnitMoved{unit.get<UnitId>().value, pos.x, pos.y});
    }

    if (pos.x == marchDest.targetX && pos.y == marchDest.targetY) {
        _eventLog.log(_tick, io::MarchEnded{unit.get<UnitId>().value, marchDest.targetX, marchDest.targetY});

        unit.remove<MarchDest>();
    }
}

} // namespace sw::systems
