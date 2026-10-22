#pragma once

#include <Systems/BaseSystem.hpp>

#include <fstream>


namespace sw::systems {

class UnitSystem : public BaseSystem {
public:
    UnitSystem(flecs::world& world, EventLog& eventLog, std::ifstream& config);
    ~UnitSystem() = default;

    void init() override;

    bool canProceed() const override { return false; }

private:
    template<typename Command>
    void processCommand(Command& command);

private:
    std::ifstream& _config;
};

} // namespace sw
