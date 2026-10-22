#pragma once

#include <distr/flecs.h>

#include <IO/System/EventLog.hpp>
#include <Systems/ISystem.hpp>


namespace sw::systems {

class BaseSystem : public ISystem {
public:
    BaseSystem(flecs::world& world, EventLog& eventLog);
    virtual ~BaseSystem() = default;

    void preUpdate() override {}

    void postUpdate() override {}

protected:
    uint64_t         _tick = 0U;
    EventLog&        _eventLog;
    flecs::world&    _world;
};

} // namespace sw::systems
