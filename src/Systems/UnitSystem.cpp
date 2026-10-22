#include "UnitSystem.hpp"

#include <IO/Commands/CreateMap.hpp>
#include <IO/Commands/SpawnSwordsman.hpp>
#include <IO/Commands/SpawnHunter.hpp>
#include <IO/Commands/March.hpp>

#include <IO/System/CommandParser.hpp>

#include <IO/Events/MapCreated.hpp>
#include <IO/Events/MarchEnded.hpp>
#include <IO/Events/MarchStarted.hpp>
#include <IO/Events/UnitSpawned.hpp>
#include <IO/Events/UnitMoved.hpp>

#include <IO/System/EventLog.hpp>
#include <IO/System/PrintDebug.hpp>

#include <Components/Components.hpp>


namespace sw::systems {

using namespace components;

static uint64_t globalEntityCounter = 0;

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

static bool canReachDestination(flecs::world& world, uint32_t x, uint32_t y) {
    if (world.has<Field>()) {
        const auto field = world.get<Field>();
        return (x < field.width && y < field.height);
    }
    return false;
}

//
// Unit System
//
UnitSystem::UnitSystem(flecs::world& world, EventLog& eventLog, std::ifstream& config) : BaseSystem(world, eventLog), _config(config) {
}

template<>
void UnitSystem::processCommand<io::CreateMap>(io::CreateMap& command) {
    // Register singleton component
    _world.component<Field>().add(flecs::Singleton);

    // Set singleton component
    _world.set<Field>({ command.width, command.height });

    _eventLog.log(_tick, io::MapCreated{command.width, command.height});

    printDebug(std::cout, command);
}

template<>
void UnitSystem::processCommand<io::SpawnSwordsman>(io::SpawnSwordsman& command) {
    if (isPositionOccupired(_world, command.x, command.y))
        return;

    auto unit = _world.entity();
    unit.add<GridPresence, Solid>();
    unit.add<CloseCombatVulnerable>();
    unit.add<DistanceCombatVulnerable>();
//    unit.add<GridPresence, Ignore>(); for Flying units or Bombs

    unit.set<UnitId>({command.unitId})
        .set<CreationOrder>({++globalEntityCounter})
        .set<Health>({command.hp})
        .set<Strength>({command.strength})
        .set<Position>({command.x, command.y});

    _eventLog.log(_tick, io::UnitSpawned{command.unitId, "Swordsman", command.x, command.y});

    printDebug(std::cout, command);
}

template<>
void UnitSystem::processCommand<io::SpawnHunter>(io::SpawnHunter& command) {
    if (isPositionOccupired(_world, command.x, command.y))
        return;

    auto unit = _world.entity();
    unit.add<GridPresence, Solid>();
    unit.add<CloseCombatVulnerable>();
    unit.add<DistanceCombatVulnerable>();

    unit.set<UnitId>({command.unitId})
        .set<CreationOrder>({++globalEntityCounter})
        .set<Health>({command.hp})
        .set<Strength>({command.strength})
        .set<Position>({command.x, command.y})
        .set<Agility>({command.agility, command.range});

    _eventLog.log(_tick, io::UnitSpawned{command.unitId, "Hunter", command.x, command.y});

    printDebug(std::cout, command);
}

template<>
void UnitSystem::processCommand<io::March>(io::March& command) {
    if (!canReachDestination(_world, command.targetX, command.targetY)) {
        std::cout << "[UnitSystem] Can't reach destination for march command [" << command.targetX << ", " << command.targetY << "]" << std::endl;
        return;
    }

    auto query = _world.query<UnitId>();
    flecs::entity unit = query.find([&command](const UnitId& uId) {
        return uId.value == command.unitId;
    });

    // Check for 'Tower' and 'Mine/Bomb' units
    if (unit.is_alive() && unit.has<Stationary>()) {
        std::cout << "[UnitSystem] Can't march stationary unit!" << std::endl;
        return;
    }

    if (unit.is_alive() && unit.has<Position>()) {
        const Position& pos = unit.get<Position>();
        unit.set<MarchDest>({pos.x, pos.y, command.targetX, command.targetY});

        printDebug(std::cout, command);
    }
}

// TODO: Add new commands here

void UnitSystem::init() {
    std::cout << "Commands:\n";
    io::CommandParser parser;
    parser.add<io::CreateMap>([this](auto command) {
            processCommand(command);
        })
        .add<io::SpawnSwordsman>([this](auto command) {
            processCommand(command);
        })
        .add<io::SpawnHunter>([this](auto command) {
            processCommand(command);
        })
        .add<io::March>([this](auto command) {
            processCommand(command);
        });
   // TODO: Add new command here

   parser.parse(_config);
}

}
