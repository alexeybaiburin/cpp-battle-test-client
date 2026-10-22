#include <fstream>
#include <iostream>

#include <Game.hpp>


using namespace sw;

int main(int argc, char** argv) {
    if (argc != 2) {
        throw std::runtime_error("Error: No file specified in command line argument");
    }

    std::ifstream file(argv[1]);
    if (!file) {
        throw std::runtime_error("Error: File not found - " + std::string(argv[1]));
    }

    Game game(file);
    game.init();

    do {
        game.step();
    } while (!game.isOver());

    return 0;
}
