#pragma once
#include <algorithm>
#include <cstdint>
#include <vector>

#include "types.h"

// Transposition table: a big array remembering what the search already learned about positions,
// indexed by the position's Zobrist key. Saves re-searching the same position reached by
// different move orders ("transpositions"), and supplies a good move to try first.

enum TTFlag : uint8_t {
    TT_NONE = 0,
    TT_UPPER = 1,  // score is an upper bound (the search failed low)
    TT_LOWER = 2,  // score is a lower bound (the search failed high / beta cutoff)
    TT_EXACT = 3,
};

struct TTEntry {
    U64 key = 0;
    Move move = NULL_MOVE;
    int16_t score = 0;
    int8_t depth = 0;
    uint8_t flag = TT_NONE;
    uint8_t age = 0;
};

class TT {
public:
    TT() { resize(256); }

    void resize(size_t megabytes) {
        size_t entries = megabytes * 1024 * 1024 / sizeof(TTEntry);
        size_t p = 1;
        while (p * 2 <= entries) p *= 2;  // power of two so we can index with a bit mask
        table.assign(p, TTEntry());
        mask = p - 1;
        age = 0;
    }
    void clear() {
        std::fill(table.begin(), table.end(), TTEntry());
        age = 0;
    }
    void newSearch() { age = (age + 1) & 63; }

    TTEntry* probe(U64 key) { return &table[key & mask]; }

    void store(U64 key, Move m, int score, int depth, int flag) {
        TTEntry& e = table[key & mask];
        if (e.key == key) {
            if (m == NULL_MOVE) m = e.move;  // keep the old best move if we have nothing better
        } else if (e.age == age && e.depth > depth + 3) {
            return;  // don't evict a much deeper result from this same search
        }
        e.key = key;
        e.move = m;
        e.score = int16_t(score);
        e.depth = int8_t(std::min(depth, 127));
        e.flag = uint8_t(flag);
        e.age = age;
    }

private:
    std::vector<TTEntry> table;
    U64 mask = 0;
    uint8_t age = 0;
};
