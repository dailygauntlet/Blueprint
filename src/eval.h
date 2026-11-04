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

// Returns the score in centipawns from the point of view of the side to move.
inline int evaluate(const Position& pos) {
    using namespace Eval;

    // Neither side can mate: bare kings, or a king plus a single minor piece.
    if (!(pos.byType[PAWN] | pos.byType[ROOK] | pos.byType[QUEEN]) &&
        popcount(pos.byType[KNIGHT] | pos.byType[BISHOP]) <= 1)
        return 0;

    int score = 0;  // White's point of view
    int kingMg = 0, kingEg = 0;
    int phase = 0;

    for (int c = WHITE; c <= BLACK; c++) {
        const int sign = (c == WHITE) ? 1 : -1;
        for (int t = PAWN; t <= QUEEN; t++) {
            U64 bb = pos.pieces(c, t);
            phase += popcount(bb) * PHASE_WEIGHT[t];
            while (bb) {
                int sq = popLsb(bb);
                int idx = (c == WHITE) ? (sq ^ 56) : sq;  // flip vertically for White (tables start at rank 8)
                score += sign * (PIECE_VALUE[t] + PST[t][idx]);
            }
        }
        int ksq = pos.kingSquare(c);
        int idx = (c == WHITE) ? (ksq ^ 56) : ksq;
        kingMg += sign * PST[KING][idx];
        kingEg += sign * KING_ENDGAME[idx];
    }

    phase = std::min(phase, TOTAL_PHASE);
    score += (kingMg * phase + kingEg * (TOTAL_PHASE - phase)) / TOTAL_PHASE;

    const int TEMPO = 10;  // small bonus for having the move
    return (pos.stm == WHITE ? score : -score) + TEMPO;
}
