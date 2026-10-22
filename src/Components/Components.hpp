#pragma once

#include <cstdint>


namespace sw::components {

struct UnitId {
    uint32_t value = 0;
};

struct CreationOrder {
    uint64_t index = 0;
};

struct Field {
    uint32_t width  = 0;
    uint32_t height = 0;
};

struct Position {
    uint32_t x = 0;
    uint32_t y = 0;

    bool operator == (const Position& other) const {
        return x == other.x && y == other.y;
    }
};

struct MarchDest {
    uint32_t startX = 0;
    uint32_t startY = 0;
    uint32_t targetX = 0;
    uint32_t targetY = 0;
};

struct Health {
    uint32_t points = 0;
};

struct Strength {
    uint32_t value = 0;
};

struct Agility {
    uint32_t value = 0;
    uint32_t range = 0;
};

struct HasAttacked {};

// Bomb/Mine has no combat (or/and Health) components
struct CloseCombatVulnerable {};
struct DistanceCombatVulnerable {};

//
// Advanced unit components
//
struct Stationary {};

struct MarchSteps {
    uint32_t value = 1;
};

struct AgilityRangeSuppression {
    uint32_t value = 1;
};

// Define a relationship pair base
struct GridPresence {};

struct Solid {};    // Standard units
struct Ignore {};   // Flying units, bombs, projectiles

}

// Helpers

static uint32_t absolute_diff(uint32_t a, uint32_t b) {
    return (a > b) ? (a - b) : (b - a);
}

int OrderComparator(uint64_t e1, const sw::components::CreationOrder* c1, uint64_t e2, const sw::components::CreationOrder* c2);
