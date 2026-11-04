#include <cstdlib>
#include <iostream>
#include <string>

#include "uci.h"

int main(int argc, char** argv) {
    initEngine();

    // Developer commands:  engine test [depth] | engine perft <depth> [fen] | engine divide <depth> [fen]
    if (argc >= 2) {
        std::string cmd = argv[1];
        if (cmd == "test") return runPerftTests(argc >= 3 ? std::atoi(argv[2]) : 4);
        if ((cmd == "perft" || cmd == "divide") && argc >= 3) {
            Position pos;
            if (argc >= 4) {
                std::string fen;
                for (int i = 3; i < argc; i++) fen += std::string(argv[i]) + " ";
                if (!pos.setFen(fen)) {
                    std::cerr << "Bad FEN\n";
                    return 1;
                }
            }
            if (cmd == "perft") timedPerft(pos, std::atoi(argv[2]));
            else divide(pos, std::atoi(argv[2]));
            return 0;
        }
    }

    UCI uci;
    uci.loop();
    return 0;
}
