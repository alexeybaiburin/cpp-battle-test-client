#include "BaseSystem.hpp"


namespace sw::systems {

BaseSystem::BaseSystem(flecs::world& world, EventLog& eventLog) :
    _world(world),
    _eventLog(eventLog) {
}

} // namespace sw::systems
