#include <IO/Commands/CreateMap.hpp>
#include <IO/Commands/March.hpp>
#include <IO/Commands/SpawnHunter.hpp>
#include <IO/Commands/SpawnSwordsman.hpp>
#include <IO/Events/MapCreated.hpp>
#include <IO/Events/MarchEnded.hpp>
#include <IO/Events/MarchStarted.hpp>
#include <IO/Events/UnitAttacked.hpp>
#include <IO/Events/UnitDied.hpp>
#include <IO/Events/UnitMoved.hpp>
#include <IO/Events/UnitSpawned.hpp>
#include <IO/System/CommandParser.hpp>
#include <IO/System/EventLog.hpp>
#include <IO/System/PrintDebug.hpp>
#include <distr/flecs.h>
#include <fstream>
#include <iostream>
#include <random>
#include <set>


struct UnitId {
    uint32_t value = 0;
};

struct CreationOrder {
    uint64_t index = 0;
};

// Sorting function for Flecs queries
int OrderComparator(flecs::entity_t e1, const CreationOrder* c1,
                    flecs::entity_t e2, const CreationOrder* c2) {
    if (c1->index < c2->index) return -1;
    if (c1->index > c2->index) return 1;
    return 0;
}

struct Position {
    uint32_t x = 0;
    uint32_t y = 0;

    bool operator==(const Position& other) const {
        return x == other.x && y == other.y;
    }
};

struct PositionHash {
    std::size_t operator()(const Position& p) const {
        return std::hash<uint32_t>()(p.x) ^ (std::hash<uint32_t>()(p.y) << 1);
    }
};

struct Point {
    int x = 0;
    int y = 0;

    bool operator==(const Point& other) const {
        return x == other.x && y == other.y;
    }
};

struct MarchDest {
    uint32_t targetX = 0;
    uint32_t targetY = 0;
    bool started = false;
};

struct Health {
    uint32_t points = 0;
};

struct Strength {
    uint32_t value = 0;
};

struct Agility {
    bool process = false;
    uint32_t value = 0;
    uint32_t range = 0;
};

struct BattleMap {
    uint32_t width = 0;
    uint32_t height = 0;

    std::vector<flecs::entity_t> grid;
//    std::unordered_map<Position, flecs::entity_t, PositionHash> grid;

    // Helper to convert 2D coordinates
    inline uint32_t getIndex(uint32_t x, uint32_t y) const { return y * width + x; }

    flecs::entity_t getUnit(uint32_t x, uint32_t y) const { return grid[getIndex(x, y)]; }

    bool isCellEmpty(uint32_t x, uint32_t y) const { return grid[getIndex(x, y)] == 0; }

    void reset(uint32_t x, uint32_t y) { grid[getIndex(x, y)] = 0; }

    void placeUnit(uint32_t x, uint32_t y, flecs::entity_t unitId) { grid[getIndex(x, y)] = unitId; }
};

BattleMap battleMap;
uint64_t entityCounter = 0;

static const std::vector<Point> directions = {
    {0, 1},  {0, -1}, {-1, 0},  {1, 0}, // Orthogonal (Up, Down, Left, Right)
    {-1, 1}, {1, 1},  {-1, -1}, {1, -1} // Diagonals (NW, NE, SW, SE)
};

std::queue<Point> calculateOpenFieldPath(const Point& start, const Point& destination) {
    std::queue<Point> pathQueue;
    Point current = start;

    // Advance one cell at a time until the destination is reached
    while (!(current == destination)) {
        // Sign function returns -1, 0, or 1 to determine direction on each axis
        int dx = (destination.x > current.x) ? 1 : ((destination.x < current.x) ? -1 : 0);
        int dy = (destination.y > current.y) ? 1 : ((destination.y < current.y) ? -1 : 0);

        current.x += dx;
        current.y += dy;

        pathQueue.push(current);
    }

    return pathQueue;
}

using namespace sw;

void createBattleMap(flecs::world& ecs, io::CreateMap& command) {
    battleMap.width = command.width;
    battleMap.height = command.height;

    battleMap.grid.clear();
    battleMap.grid.resize(command.width * command.height, 0);

    printDebug(std::cout, command);
}

void addSpawnSwordsman(flecs::world& ecs, io::SpawnSwordsman& command) {
    if (!battleMap.isCellEmpty(command.x, command.y)) {
        return;
    }

    auto unit = ecs.entity();
    unit.set<UnitId>({command.unitId})
        .set<CreationOrder>({++entityCounter})
        .set<Health>({command.hp})
        .set<Strength>({command.strength})
        .set<Position>({command.x, command.y});

    battleMap.placeUnit(command.x, command.y, unit.id());

    printDebug(std::cout, command);
}

void addSpawnHunter(flecs::world& ecs, io::SpawnHunter& command) {
    if (!battleMap.isCellEmpty(command.x, command.y)) {
        return;
    }

    auto unit = ecs.entity();
    unit.set<UnitId>({command.unitId})
        .set<CreationOrder>({++entityCounter})
        .set<Health>({command.hp})
        .set<Strength>({command.strength})
        .set<Position>({command.x, command.y})
        .set<Agility>({true, command.agility, command.range});

    battleMap.placeUnit(command.x, command.y, unit.id());

    printDebug(std::cout, command);
}

void addMarchDestination(flecs::world& ecs, io::March& command) {
    flecs::entity unit = ecs.entity(command.unitId);
    if (unit.is_alive()) {
        unit.set<MarchDest>({command.targetX, command.targetY});

        printDebug(std::cout, command);
    }
}

int main(int argc, char** argv) {
    if (argc != 2) {
        throw std::runtime_error("Error: No file specified in command line argument");
    }

    std::ifstream file(argv[1]);
    if (!file) {
        throw std::runtime_error("Error: File not found - " + std::string(argv[1]));
    }

    EventLog eventLog;
    flecs::world ecs;

    // Register close combat system
    ecs.system<const Position, const Health, const Strength, const CreationOrder>("CloseCombatSystem")
        .order_by<CreationOrder>(OrderComparator)
        .each([&eventLog](flecs::entity attacker, const Position& pos, const Health& health, const Strength& strength, const CreationOrder&) {
            std::vector<flecs::entity_t> neighbors;

            Point current(pos.x, pos.y);
            for (const auto& dir : directions) {
                Point neighbor = {current.x + dir.x, current.y + dir.y};
                // Skip out of bounds neighbors
                if (neighbor.x < 0 || neighbor.x >= battleMap.width || neighbor.y < 0 || neighbor.y >= battleMap.height)
                    continue;

                // TODO: check if neighbor is alive
                if (!battleMap.isCellEmpty(neighbor.x, neighbor.y)) {
                    neighbors.push_back(battleMap.getUnit(neighbor.x, neighbor.y));
                }
            }

            flecs::world world = attacker.world();

            if (!neighbors.empty()) {
                std::random_device rd;
                std::mt19937 gen(rd());
                std::uniform_int_distribution<size_t> distr(0, neighbors.size() - 1);

                // 3. Generate a random index and access the element
                size_t idx = distr(gen);
                flecs::entity_t targetId = neighbors[idx];
                flecs::entity targetUnit = world.entity(targetId);
                if (targetUnit.is_alive()) {
                    const UnitId& unitId = targetUnit.get<UnitId>();
                    const Position& pos = targetUnit.get<Position>();
                    Health& h = targetUnit.get_mut<Health>();
                    if (strength.value >= h.points) {
                        h.points = 0;
                        targetUnit.destruct();

                        battleMap.reset(pos.x, pos.y); // erase from field
                    }
                    else {
                        h.points -= strength.value;
                    }

                    Agility* agility = attacker.try_get_mut<Agility>();
                    if (nullptr != agility) {
                        // Cancel agility if attacker is already hit damage
                        agility->process = false;
                    }

                    // Attack event first
                    const UnitId& attackerId = attacker.get<UnitId>();
                    eventLog.log(8, io::UnitAttacked{
                        static_cast<uint32_t>(attackerId.value),
                        static_cast<uint32_t>(unitId.value),
                        strength.value,
                        h.points
                    });

                    // Death event second
                    if (h.points == 0) {
                        eventLog.log(4, io::UnitDied{unitId.value});
                    }
                }
            }
        });

    ecs.system<const Position, const Health, const Strength, const Agility, const CreationOrder>("AgilityCombatSystem")
        .order_by<CreationOrder>(OrderComparator)
        .each([&eventLog](flecs::entity attacker, const Position& pos, const Health& health, const Strength& strength, const Agility& agility, const CreationOrder&) {
            if (!agility.process)
                return;

            uint32_t min_x = (pos.x >= agility.range) ? (pos.x - agility.range) : 0U;
            uint32_t max_x = std::min(battleMap.width - 1, pos.x + agility.range);
            uint32_t min_y = (pos.y >= agility.range) ? (pos.y - agility.range) : 0U;
            uint32_t max_y = std::min(battleMap.height - 1, pos.y + agility.range);

            auto world = attacker.world();
            auto q = world.query<Position>();

            std::vector<flecs::entity_t> neighbors;

            q.each([&](flecs::entity e, Position& p) {
                // Skip the source unit itself
                if (pos.x == p.x && pos.y == p.y) {
                    return;
                }

                // Fast rejection: Check if the unit is inside the localized bounding box
                if (p.x >= min_x && p.x <= max_x && p.y >= min_y && p.y <= max_y) {
                    // Calculate Chebyshev distance (includes diagonals)
                    uint32_t dx = std::abs(static_cast<long long>(p.x - pos.x));
                    uint32_t dy = std::abs(static_cast<long long>(p.y - pos.y));
                    uint32_t distance = std::max(dx, dy);

                    // Filter out units closer than or equal to 2 cells
                    if (distance > 2) {
                        neighbors.push_back(e.id());
                    }
                }
            });

            if (!neighbors.empty()) {
                std::random_device rd;
                std::mt19937 gen(rd());
                std::uniform_int_distribution<size_t> distr(0, neighbors.size() - 1);

                // 3. Generate a random index and access the element
                size_t idx = distr(gen);
                flecs::entity_t targetId = neighbors[idx];
                flecs::entity targetUnit = world.entity(targetId);
                if (targetUnit.is_alive()) {
                    const UnitId& unitId = targetUnit.get<UnitId>();
                    const Position& pos = targetUnit.get<Position>();
                    Health& h = targetUnit.get_mut<Health>();
                    if (agility.value >= h.points) {
                        h.points = 0;
                        targetUnit.destruct();

                        battleMap.reset(pos.x, pos.y); // erase from field
                    }
                    else {
                        h.points -= agility.value;
                    }

                    // Attack event first
                    const UnitId& attackerId = attacker.get<UnitId>();
                    eventLog.log(8, io::UnitAttacked{
                        static_cast<uint32_t>(attackerId.value),
                        static_cast<uint32_t>(unitId.value),
                        agility.value,
                        h.points
                    });

                    // Death event second
                    if (h.points == 0) {
                        eventLog.log(4, io::UnitDied{unitId.value});
                    }
                }
            }
        });

    std::cout << "Commands:\n";
    io::CommandParser parser;
    parser.add<io::CreateMap>([&ecs](auto command) {
            createBattleMap(ecs, command);
        })
        .add<io::SpawnSwordsman>([&ecs](auto command) {
            addSpawnSwordsman(ecs, command);
        })
        .add<io::SpawnHunter>([&ecs](auto command) {
            addSpawnHunter(ecs, command);
        })
        .add<io::March>([&ecs](auto command) {
            addMarchDestination(ecs, command);
        });

    parser.parse(file);

    for (size_t i = 0; i < 10; ++i) {
        ecs.progress();
    }

    std::cout << "\n\nEvents:\n";

    eventLog.log(1, io::MapCreated{10, 10});
    eventLog.log(1, io::UnitSpawned{1, "Swordsman", 0, 0});
    eventLog.log(1, io::UnitSpawned{2, "Hunter", 9, 0});
    eventLog.log(1, io::MarchStarted{1, 0, 0, 9, 0});
    eventLog.log(1, io::MarchStarted{2, 9, 0, 0, 0});
    eventLog.log(1, io::UnitSpawned{3, "Swordsman", 0, 9});
    eventLog.log(1, io::MarchStarted{3, 0, 9, 0, 0});

    eventLog.log(2, io::UnitMoved{1, 1, 0});
    eventLog.log(2, io::UnitMoved{2, 8, 0});
    eventLog.log(2, io::UnitMoved{3, 0, 8});

    eventLog.log(3, io::UnitMoved{1, 2, 0});
    eventLog.log(3, io::UnitMoved{2, 7, 0});
    eventLog.log(3, io::UnitMoved{3, 0, 7});

    eventLog.log(4, io::UnitMoved{1, 3, 0});
    eventLog.log(4, io::UnitAttacked{2, 1, 5, 0});
    eventLog.log(4, io::UnitDied{1});
    eventLog.log(4, io::UnitMoved{3, 0, 6});

    eventLog.log(5, io::UnitMoved{2, 6, 0});
    eventLog.log(5, io::UnitMoved{3, 0, 5});

    eventLog.log(6, io::UnitMoved{2, 5, 0});
    eventLog.log(6, io::UnitMoved{3, 0, 4});

    eventLog.log(7, io::UnitAttacked{2, 3, 5, 5});
    eventLog.log(7, io::UnitMoved{3, 0, 3});

    eventLog.log(8, io::UnitAttacked{2, 3, 5, 0});
    eventLog.log(8, io::UnitDied{3});

    return 0;
}
