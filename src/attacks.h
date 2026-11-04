#pragma once
#include <vector>

#include "types.h"

// Attack tables for all piece types. Call Attacks::init() once at startup.

struct Magic {
    U64 mask;
    U64 magic;
    U64* attacks;
    int shift;
};

inline U64 knightAttacks[64];
inline U64 kingAttacks[64];
inline U64 pawnAttacks[2][64];  // pawnAttacks[c][sq]: squares attacked by a pawn of colour c on sq
inline Magic rookMagics[64];
inline Magic bishopMagics[64];
inline U64 rookTable[102400];
inline U64 bishopTable[5248];

inline U64 rookAttacks(int sq, U64 occ) {
    const Magic& m = rookMagics[sq];
    return m.attacks[((occ & m.mask) * m.magic) >> m.shift];
}
inline U64 bishopAttacks(int sq, U64 occ) {
    const Magic& m = bishopMagics[sq];
    return m.attacks[((occ & m.mask) * m.magic) >> m.shift];
}
inline U64 queenAttacks(int sq, U64 occ) { return rookAttacks(sq, occ) | bishopAttacks(sq, occ); }

namespace attacks_detail {

inline const int ROOK_DIRS[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
inline const int BISHOP_DIRS[4][2] = {{1, 1}, {1, -1}, {-1, 1}, {-1, -1}};

// Slow ray-walking reference; only used at startup to build the magic tables.
inline U64 slidingAttack(int sq, U64 occ, bool rook) {
    const int(*dirs)[2] = rook ? ROOK_DIRS : BISHOP_DIRS;
    U64 a = 0;
    for (int d = 0; d < 4; d++) {
        int f = sq % 8 + dirs[d][0], r = sq / 8 + dirs[d][1];
        while (f >= 0 && f < 8 && r >= 0 && r < 8) {
            a |= bit(r * 8 + f);
            if (occ & bit(r * 8 + f)) break;
            f += dirs[d][0];
            r += dirs[d][1];
        }
    }
    return a;
}

// Squares whose occupancy can change the attack set (board edges excluded).
inline U64 relevantMask(int sq, bool rook) {
    const int(*dirs)[2] = rook ? ROOK_DIRS : BISHOP_DIRS;
    U64 m = 0;
    for (int d = 0; d < 4; d++) {
        int f = sq % 8 + dirs[d][0], r = sq / 8 + dirs[d][1];
        while (f + dirs[d][0] >= 0 && f + dirs[d][0] < 8 && r + dirs[d][1] >= 0 && r + dirs[d][1] < 8) {
            m |= bit(r * 8 + f);
            f += dirs[d][0];
            r += dirs[d][1];
        }
    }
    return m;
}

inline U64 rngState = 0x9E3779B97F4A7C15ULL;
inline U64 rng() {  // xorshift64*
    rngState ^= rngState >> 12;
    rngState ^= rngState << 25;
    rngState ^= rngState >> 27;
    return rngState * 2685821657736338717ULL;
}

inline void initMagics(bool rook, Magic* magics, U64* table) {
    size_t offset = 0;
    for (int sq = 0; sq < 64; sq++) {
        U64 mask = relevantMask(sq, rook);
        int bits = popcount(mask);
        size_t size = size_t(1) << bits;

        std::vector<U64> occs(size), ref(size);
        U64 b = 0;
        size_t n = 0;
        do {  // carry-rippler enumeration of all subsets of mask
            occs[n] = b;
            ref[n] = slidingAttack(sq, b, rook);
            n++;
            b = (b - mask) & mask;
        } while (b);

        Magic& m = magics[sq];
        m.mask = mask;
        m.shift = 64 - bits;
        m.attacks = table + offset;

        for (;;) {
            U64 magic = rng() & rng() & rng();  // sparse candidates work best
            if (popcount((mask * magic) >> 56) < 6) continue;
            for (size_t i = 0; i < size; i++) m.attacks[i] = 0;  // attack sets are never empty
            bool ok = true;
            for (size_t i = 0; i < size && ok; i++) {
                size_t idx = (occs[i] * magic) >> m.shift;
                if (m.attacks[idx] == 0) m.attacks[idx] = ref[i];
                else if (m.attacks[idx] != ref[i]) ok = false;
            }
            if (ok) {
                m.magic = magic;
                break;
            }
        }
        offset += size;
    }
}

}  // namespace attacks_detail

namespace Attacks {

inline void init() {
    for (int sq = 0; sq < 64; sq++) {
        int f = sq % 8, r = sq / 8;
        U64 n = 0, k = 0;
        const int kn[8][2] = {{1, 2}, {2, 1}, {2, -1}, {1, -2}, {-1, -2}, {-2, -1}, {-2, 1}, {-1, 2}};
        for (auto& d : kn) {
            int nf = f + d[0], nr = r + d[1];
            if (nf >= 0 && nf < 8 && nr >= 0 && nr < 8) n |= bit(nr * 8 + nf);
        }
        for (int df = -1; df <= 1; df++)
            for (int dr = -1; dr <= 1; dr++) {
                if (!df && !dr) continue;
                int nf = f + df, nr = r + dr;
                if (nf >= 0 && nf < 8 && nr >= 0 && nr < 8) k |= bit(nr * 8 + nf);
            }
        knightAttacks[sq] = n;
        kingAttacks[sq] = k;

        U64 wp = 0, bp = 0;
        if (r < 7) {
            if (f > 0) wp |= bit(sq + 7);
            if (f < 7) wp |= bit(sq + 9);
        }
        if (r > 0) {
            if (f > 0) bp |= bit(sq - 9);
            if (f < 7) bp |= bit(sq - 7);
        }
        pawnAttacks[WHITE][sq] = wp;
        pawnAttacks[BLACK][sq] = bp;
    }
    attacks_detail::initMagics(true, rookMagics, rookTable);
    attacks_detail::initMagics(false, bishopMagics, bishopTable);
}

}  // namespace Attacks
