#include "Game.hpp"

#include <Components/Components.hpp>

#include <Systems/CombatSystem.hpp>
#include <Systems/MovementSystem.hpp>
#include <Systems/UnitSystem.hpp>

#include <fstream>
#include <iostream>


namespace sw {

//
// Game
//
Game::Game(std::ifstream& config) {
    _systems.emplace_back(std::make_shared<systems::UnitSystem>(_world, _eventLog, config));
    _systems.emplace_back(std::make_shared<systems::CombatSystem>(_world, _eventLog));
    // _systems.emplace_back(std::make_shared<systems::HealingSystem>(_world, _eventLog));
    _systems.emplace_back(std::make_shared<systems::MovementSystem>(_world, _eventLog));
    // _systems.emplace_back(std::make_shared<systems::AreaSplashSystem>(_world, _eventLog));
}

Game::~Game() = default;

void Game::init() {
    // Can be several stages: preInit, init, postInit
    // to register flecs systems in different order
    for (auto& system : _systems) {
        if (system) system->init();
    }
}

bool Game::isOver() const {
    // 1. Only one unit left
    if (1 == _world.count<components::UnitId>()) {
        return true;
    }

    bool isOver = std::none_of(_systems.begin(), _systems.end(), [](const auto& system) {
        return system->canProceed(); 
    });

    if (isOver) {
        std::cout << "Game over!" << std::endl;
        return true;
    }

    // Prevent hanging
    return _tick >= 1000U;
}

void Game::step() {
    for (auto& system : _systems) {
        if (system) system->preUpdate();
    }

    _world.progress();

    for (auto& system : _systems) {
        if (system) system->postUpdate();
    }

    ++_tick;
}

} // namespace sw
