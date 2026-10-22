#pragma once

#include <memory>
#include <vector>
#include <fstream>

#include <distr/flecs.h>

#include <IO/System/EventLog.hpp>
#include <Systems/ISystem.hpp>


namespace sw {

class Game {
public:
    using System = std::shared_ptr<systems::ISystem>;
public:
    Game(std::ifstream& config);
    ~Game();

    void init();

    bool isOver() const;

    void step();

private:
    std::vector<System> _systems;

    uint64_t        _tick = 0U;
    EventLog        _eventLog;
    flecs::world    _world;
};

}
