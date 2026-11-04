#pragma once
#include <chrono>
#include <iostream>
#include <string>
#include <vector>

#include "position.h"

// Perft: counts all legal move sequences of a given length. Used to verify the move generator.

inline U64 perft(Position& pos, int depth) {
    if (depth == 0) return 1;
    MoveList list;
    pos.generate(list);
    U64 nodes = 0;
    for (int i = 0; i < list.count; i++) {
        State st;
        pos.makeMove(list.moves[i], st);
        if (pos.lastMoveLegal()) nodes += (depth == 1) ? 1 : perft(pos, depth - 1);
        pos.unmakeMove(list.moves[i], st);
    }
    return nodes;
}

// Like perft, but prints the node count under each root move (handy for finding bugs).
inline void divide(Position& pos, int depth) {
    MoveList list;
    pos.generate(list);
    U64 total = 0;
    for (int i = 0; i < list.count; i++) {
        State st;
        pos.makeMove(list.moves[i], st);
        if (pos.lastMoveLegal()) {
            U64 n = (depth <= 1) ? 1 : perft(pos, depth - 1);
            std::cout << moveToString(list.moves[i]) << ": " << n << "\n";
            total += n;
        }
        pos.unmakeMove(list.moves[i], st);
    }
    std::cout << "\nNodes: " << total << std::endl;
}

inline void timedPerft(Position& pos, int depth) {
    auto t0 = std::chrono::steady_clock::now();
    U64 n = perft(pos, depth);
    double secs = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
    std::cout << "Nodes: " << n << "  Time: " << secs << "s  NPS: " << (secs > 0 ? U64(n / secs) : 0) << std::endl;
}

struct PerftCase {
    const char* name;
    const char* fen;
    std::vector<U64> expected;  // expected[d-1] = nodes at depth d
};

inline int runPerftTests(int maxDepth) {
    const std::vector<PerftCase> cases = {
        {"startpos", Position::START_FEN, {20, 400, 8902, 197281, 4865609, 119060324}},
        {"kiwipete", "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1",
         {48, 2039, 97862, 4085603, 193690690}},
        {"pos3", "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1", {14, 191, 2812, 43238, 674624, 11030083}},
        {"pos4", "r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1",
         {6, 264, 9467, 422333, 15833292}},
        {"pos5", "rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8", {44, 1486, 62379, 2103487, 89941194}},
        {"pos6", "r4rk1/1pp1qppp/p1np1n2/2b1p1B1/2B1P1b1/P1NP1N2/1PP1QPPP/R4RK1 w - - 0 10",
         {46, 2079, 89890, 3894594, 164075551}},
    };
    int failures = 0;
    for (const auto& c : cases) {
        Position pos;
        pos.setFen(c.fen);
        for (int d = 1; d <= maxDepth && d <= (int)c.expected.size(); d++) {
            U64 got = perft(pos, d);
            bool ok = got == c.expected[d - 1];
            std::cout << (ok ? "PASS " : "FAIL ") << c.name << " depth " << d << ": " << got;
            if (!ok) std::cout << " (expected " << c.expected[d - 1] << ")";
            std::cout << "\n";
            if (!ok) failures++;
        }
    }
    if (failures) std::cout << "FAILURES: " << failures << std::endl;
    else std::cout << "All perft tests passed." << std::endl;
    return failures ? 1 : 0;
}
