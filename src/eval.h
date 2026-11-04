#pragma once
#include <algorithm>

#include "position.h"

// Static evaluation: material + piece-square tables (PSTs), with the king blending between a
// "hide in the corner" table (middlegame) and a "go to the centre" table (endgame).
// Tables are written the way you'd see a board: first row = rank 8, last row = rank 1,
// from White's point of view. Values are in centipawns (100 = one pawn).

namespace Eval {

inline constexpr int PIECE_VALUE[6] = {100, 320, 330, 500, 900, 0};
inline constexpr int PHASE_WEIGHT[6] = {0, 1, 1, 2, 4, 0};  // how much each piece counts toward "middlegame-ness"
inline constexpr int TOTAL_PHASE = 24;

inline constexpr int PST[6][64] = {
    // Pawn
    {0,  0,  0,  0,   0,   0,  0,  0,   //
     50, 50, 50, 50,  50,  50, 50, 50,  //
     10, 10, 20, 30,  30,  20, 10, 10,  //
     5,  5,  10, 25,  25,  10, 5,  5,   //
     0,  0,  0,  20,  20,  0,  0,  0,   //
     5,  -5, -10, 0,  0,  -10, -5, 5,   //
     5,  10, 10, -20, -20, 10, 10, 5,   //
     0,  0,  0,  0,   0,   0,  0,  0},
    // Knight
    {-50, -40, -30, -30, -30, -30, -40, -50,  //
     -40, -20, 0,   0,   0,   0,   -20, -40,  //
     -30, 0,   10,  15,  15,  10,  0,   -30,  //
     -30, 5,   15,  20,  20,  15,  5,   -30,  //
     -30, 0,   15,  20,  20,  15,  0,   -30,  //
     -30, 5,   10,  15,  15,  10,  5,   -30,  //
     -40, -20, 0,   5,   5,   0,   -20, -40,  //
     -50, -40, -30, -30, -30, -30, -40, -50},
    // Bishop
    {-20, -10, -10, -10, -10, -10, -10, -20,  //
     -10, 0,   0,   0,   0,   0,   0,   -10,  //
     -10, 0,   5,   10,  10,  5,   0,   -10,  //
     -10, 5,   5,   10,  10,  5,   5,   -10,  //
     -10, 0,   10,  10,  10,  10,  0,   -10,  //
     -10, 10,  10,  10,  10,  10,  10,  -10,  //
     -10, 5,   0,   0,   0,   0,   5,   -10,  //
     -20, -10, -10, -10, -10, -10, -10, -20},
    // Rook
    {0,  0,  0,  0,  0,  0,  0,  0,  //
     5,  10, 10, 10, 10, 10, 10, 5,  //
     -5, 0,  0,  0,  0,  0,  0,  -5, //
     -5, 0,  0,  0,  0,  0,  0,  -5, //
     -5, 0,  0,  0,  0,  0,  0,  -5, //
     -5, 0,  0,  0,  0,  0,  0,  -5, //
     -5, 0,  0,  0,  0,  0,  0,  -5, //
     0,  0,  0,  5,  5,  0,  0,  0},
    // Queen
    {-20, -10, -10, -5, -5, -10, -10, -20,  //
     -10, 0,   0,   0,  0,  0,   0,   -10,  //
     -10, 0,   5,   5,  5,  5,   0,   -10,  //
     -5,  0,   5,   5,  5,  5,   0,   -5,   //
     0,   0,   5,   5,  5,  5,   0,   -5,   //
     -10, 5,   5,   5,  5,  5,   0,   -10,  //
     -10, 0,   5,   0,  0,  0,   0,   -10,  //
     -20, -10, -10, -5, -5, -10, -10, -20},
    // King (middlegame)
    {-30, -40, -40, -50, -50, -40, -40, -30,  //
     -30, -40, -40, -50, -50, -40, -40, -30,  //
     -30, -40, -40, -50, -50, -40, -40, -30,  //
     -30, -40, -40, -50, -50, -40, -40, -30,  //
     -20, -30, -30, -40, -40, -30, -30, -20,  //
     -10, -20, -20, -20, -20, -20, -20, -10,  //
     20,  20,  0,   0,   0,   0,   20,  20,   //
     20,  30,  10,  0,   0,   10,  30,  20},
};

inline constexpr int KING_ENDGAME[64] = {
    -50, -40, -30, -20, -20, -30, -40, -50,  //
    -30, -20, -10, 0,   0,   -10, -20, -30,  //
    -30, -10, 20,  30,  30,  20,  -10, -30,  //
    -30, -10, 30,  40,  40,  30,  -10, -30,  //
    -30, -10, 30,  40,  40,  30,  -10, -30,  //
    -30, -10, 20,  30,  30,  20,  -10, -30,  //
    -30, -30, 0,   0,   0,   0,   -30, -30,  //
    -50, -30, -30, -30, -30, -30, -30, -50};

}  // namespace Eval

// ---- extra evaluation terms (all values in centipawns) ----
namespace Eval {

inline constexpr U64 FILE_A = 0x0101010101010101ULL;
inline constexpr U64 FILE_H = FILE_A << 7;

// Bonus for a passed pawn (no enemy pawn in front of it or on the adjacent files), by how far it has advanced.
inline constexpr int PASSED_MG[8] = {0, 0, 5, 10, 20, 40, 70, 0};
inline constexpr int PASSED_EG[8] = {0, 10, 20, 35, 60, 100, 160, 0};

inline constexpr int DOUBLED_MG = -8, DOUBLED_EG = -12;    // per extra pawn on a file
inline constexpr int ISOLATED_MG = -10, ISOLATED_EG = -12;  // pawn with no friendly pawn on adjacent files
inline constexpr int BISHOP_PAIR_MG = 30, BISHOP_PAIR_EG = 45;
inline constexpr int ROOK_OPEN_MG = 20, ROOK_OPEN_EG = 10;  // no pawns at all on the file
inline constexpr int ROOK_SEMI_MG = 10, ROOK_SEMI_EG = 8;   // no friendly pawns on the file

// Mobility: (safe squares reachable - baseline) * weight, per piece type [knight, bishop, rook, queen]
inline constexpr int MOB_BASE[4] = {4, 5, 6, 12};
inline constexpr int MOB_MG[4] = {4, 3, 2, 1};
inline constexpr int MOB_EG[4] = {4, 3, 4, 2};

struct Tables {
    U64 passed[2][64];  // squares that must be free of enemy pawns for a pawn to be passed
    U64 adjacent[8];    // the files next to each file
    Tables() {
        for (int sq = 0; sq < 64; sq++) {
            int f = sq % 8, r = sq / 8;
            U64 w = 0, b = 0;
            for (int ff = std::max(0, f - 1); ff <= std::min(7, f + 1); ff++) {
                for (int rr = r + 1; rr < 8; rr++) w |= bit(rr * 8 + ff);
                for (int rr = r - 1; rr >= 0; rr--) b |= bit(rr * 8 + ff);
            }
            passed[WHITE][sq] = w;
            passed[BLACK][sq] = b;
        }
        for (int f = 0; f < 8; f++) {
            U64 m = 0;
            if (f > 0) m |= FILE_A << (f - 1);
            if (f < 7) m |= FILE_A << (f + 1);
            adjacent[f] = m;
        }
    }
};
inline const Tables TABLES;

inline U64 pawnAttackBB(U64 pawns, int color) {
    if (color == WHITE) return ((pawns << 7) & ~FILE_H) | ((pawns << 9) & ~FILE_A);
    return ((pawns >> 7) & ~FILE_A) | ((pawns >> 9) & ~FILE_H);
}

// Pawns in front of the king (middlegame only): reward a shield, punish holes.
inline int kingShield(const Position& pos, int c) {
    int ksq = pos.kingSquare(c), kf = ksq % 8, kr = ksq / 8;
    int rel = (c == WHITE) ? kr : 7 - kr;
    if (rel > 1) return 0;
    const int dir = (c == WHITE) ? 1 : -1;
    const U64 own = pos.pieces(c, PAWN);
    int s = 0;
    for (int ff = std::max(0, kf - 1); ff <= std::min(7, kf + 1); ff++) {
        if (own & bit((kr + dir) * 8 + ff)) s += 12;
        else if (own & bit((kr + 2 * dir) * 8 + ff)) s += 6;
        else s -= 12;
    }
    return s;
}

}  // namespace Eval

// Returns the score in centipawns from the point of view of the side to move.
inline int evaluate(const Position& pos) {
    using namespace Eval;

    // Neither side can mate: bare kings, or a king plus a single minor piece.
    if (!(pos.byType[PAWN] | pos.byType[ROOK] | pos.byType[QUEEN]) &&
        popcount(pos.byType[KNIGHT] | pos.byType[BISHOP]) <= 1)
        return 0;

    const U64 occ = pos.occupied();
    const U64 pawnAtt[2] = {pawnAttackBB(pos.pieces(WHITE, PAWN), WHITE), pawnAttackBB(pos.pieces(BLACK, PAWN), BLACK)};

    int mg = 0, eg = 0;  // White's point of view
    int phase = 0;

    for (int c = WHITE; c <= BLACK; c++) {
        const int sign = (c == WHITE) ? 1 : -1;
        const int them = c ^ 1;
        const U64 ownPawns = pos.pieces(c, PAWN);
        const U64 enemyPawns = pos.pieces(them, PAWN);
        const U64 safe = ~(pos.byColor[c] | pawnAtt[them]);  // squares a piece can usefully go to
        int cmg = 0, ceg = 0;                                // this colour's score

        // Pawns: material, placement, passed / doubled / isolated
        U64 bb = ownPawns;
        while (bb) {
            int sq = popLsb(bb);
            int idx = (c == WHITE) ? (sq ^ 56) : sq;
            int f = sq % 8;
            int rel = (c == WHITE) ? sq / 8 : 7 - sq / 8;
            cmg += PIECE_VALUE[PAWN] + PST[PAWN][idx];
            ceg += PIECE_VALUE[PAWN] + PST[PAWN][idx];
            if (!(TABLES.passed[c][sq] & enemyPawns)) {
                cmg += PASSED_MG[rel];
                ceg += PASSED_EG[rel];
            }
            if (!(ownPawns & TABLES.adjacent[f])) {
                cmg += ISOLATED_MG;
                ceg += ISOLATED_EG;
            }
        }
        for (int f = 0; f < 8; f++) {
            int n = popcount(ownPawns & (FILE_A << f));
            if (n > 1) {
                cmg += DOUBLED_MG * (n - 1);
                ceg += DOUBLED_EG * (n - 1);
            }
        }

        // Knights, bishops, rooks, queens: material, placement, mobility
        for (int t = KNIGHT; t <= QUEEN; t++) {
            bb = pos.pieces(c, t);
            phase += popcount(bb) * PHASE_WEIGHT[t];
            while (bb) {
                int sq = popLsb(bb);
                int idx = (c == WHITE) ? (sq ^ 56) : sq;
                U64 att;
                switch (t) {
                    case KNIGHT: att = knightAttacks[sq]; break;
                    case BISHOP: att = bishopAttacks(sq, occ); break;
                    case ROOK: att = rookAttacks(sq, occ); break;
                    default: att = queenAttacks(sq, occ); break;
                }
                int m = t - KNIGHT;  // 0..3
                int mob = popcount(att & safe) - MOB_BASE[m];
                cmg += PIECE_VALUE[t] + PST[t][idx] + mob * MOB_MG[m];
                ceg += PIECE_VALUE[t] + PST[t][idx] + mob * MOB_EG[m];
                if (t == ROOK) {
                    U64 file = FILE_A << (sq % 8);
                    if (!(file & (ownPawns | enemyPawns))) {
                        cmg += ROOK_OPEN_MG;
                        ceg += ROOK_OPEN_EG;
                    } else if (!(file & ownPawns)) {
                        cmg += ROOK_SEMI_MG;
                        ceg += ROOK_SEMI_EG;
                    }
                }
            }
        }
        if (popcount(pos.pieces(c, BISHOP)) >= 2) {
            cmg += BISHOP_PAIR_MG;
            ceg += BISHOP_PAIR_EG;
        }

        // King
        int ksq = pos.kingSquare(c);
        int kidx = (c == WHITE) ? (ksq ^ 56) : ksq;
        cmg += PST[KING][kidx] + kingShield(pos, c);
        ceg += KING_ENDGAME[kidx];

        mg += sign * cmg;
        eg += sign * ceg;
    }

    phase = std::min(phase, TOTAL_PHASE);
    int score = (mg * phase + eg * (TOTAL_PHASE - phase)) / TOTAL_PHASE;

    const int TEMPO = 10;  // small bonus for having the move
    return (pos.stm == WHITE ? score : -score) + TEMPO;
}
