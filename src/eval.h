#pragma once
#include <algorithm>
#include <vector>

#include "eval_tuned.h"
#include "position.h"

// Static evaluation: material + piece-square tables (PSTs) + positional terms, with every value
// kept in one parameter block (Eval::Params) so it can be tuned automatically (see tools/tune.cpp).
// Each term has a middlegame and an endgame value; the two are blended by how much material is left.
// Tables are written the way you'd see a board: first row = rank 8, last row = rank 1, from White's side.
// Values are in centipawns (roughly: 100 = one pawn).

namespace Eval {

inline constexpr U64 FILE_A = 0x0101010101010101ULL;
inline constexpr U64 FILE_H = FILE_A << 7;
inline constexpr int PHASE_WEIGHT[6] = {0, 1, 1, 2, 4, 0};  // how much each piece counts toward "middlegame-ness"
inline constexpr int TOTAL_PHASE = 24;
inline constexpr int MOB_BASE[4] = {4, 5, 6, 12};  // "average" number of safe squares: knight, bishop, rook, queen
inline constexpr int NPM_VALUE[6] = {0, 320, 330, 500, 900, 0};  // for the drawn-endgame test
inline constexpr int KS_MAX = 400;                               // cap on the king-danger penalty

struct Params {
    int matMg[6], matEg[6];
    int pstMg[6][64], pstEg[6][64];
    int mobMg[4], mobEg[4];  // per safe square above MOB_BASE: knight, bishop, rook, queen
    int passedMg[8], passedEg[8];
    int supportedMg[8], supportedEg[8];  // pawn defended by a pawn, by (relative) rank
    int phalanxMg[8], phalanxEg[8];      // pawn with a friendly pawn beside it
    int doubledMg, doubledEg;            // per extra pawn on a file
    int isolatedMg, isolatedEg;
    int bishopPairMg, bishopPairEg;
    int rookOpenMg, rookOpenEg;  // no pawns on the file
    int rookSemiMg, rookSemiEg;  // no friendly pawns on the file
    int rook7thMg, rook7thEg;
    int outpostMg, outpostEg;  // knight on a pawn-supported square no enemy pawn can attack
    int threatPawnMg[4], threatPawnEg[4];  // our pawn attacks an enemy knight, bishop, rook, queen
    int kingShield[3];                     // pawn one step in front / two steps in front / missing, per king file
    int kingFileOpen, kingFileSemi;        // open / half-open file next to the king (enemy has rook or queen)
    int ksWeight[4];                       // king attack weight per attacking knight, bishop, rook, queen
    int ksScale;                           // king danger = units^2 * ksScale / 256
    int tempo;
};

// Calls f(value, tunable) for every parameter in a fixed order. The tuner and the saved parameter
// files both rely on this order.
template <class F>
void forEachParam(Params& p, F&& f) {
    for (int t = 0; t < 6; t++) f(p.matMg[t], t >= KNIGHT && t <= QUEEN);
    for (int t = 0; t < 6; t++) f(p.matEg[t], t >= KNIGHT && t <= QUEEN);
    for (int t = 0; t < 6; t++)
        for (int s = 0; s < 64; s++) f(p.pstMg[t][s], t != PAWN || (s >= 8 && s < 56));
    for (int t = 0; t < 6; t++)
        for (int s = 0; s < 64; s++) f(p.pstEg[t][s], t != PAWN || (s >= 8 && s < 56));
    for (int i = 0; i < 4; i++) f(p.mobMg[i], true);
    for (int i = 0; i < 4; i++) f(p.mobEg[i], true);
    for (int r = 0; r < 8; r++) f(p.passedMg[r], r >= 1 && r <= 6);
    for (int r = 0; r < 8; r++) f(p.passedEg[r], r >= 1 && r <= 6);
    for (int r = 0; r < 8; r++) f(p.supportedMg[r], r >= 1 && r <= 6);
    for (int r = 0; r < 8; r++) f(p.supportedEg[r], r >= 1 && r <= 6);
    for (int r = 0; r < 8; r++) f(p.phalanxMg[r], r >= 1 && r <= 6);
    for (int r = 0; r < 8; r++) f(p.phalanxEg[r], r >= 1 && r <= 6);
    f(p.doubledMg, true);
    f(p.doubledEg, true);
    f(p.isolatedMg, true);
    f(p.isolatedEg, true);
    f(p.bishopPairMg, true);
    f(p.bishopPairEg, true);
    f(p.rookOpenMg, true);
    f(p.rookOpenEg, true);
    f(p.rookSemiMg, true);
    f(p.rookSemiEg, true);
    f(p.rook7thMg, true);
    f(p.rook7thEg, true);
    f(p.outpostMg, true);
    f(p.outpostEg, true);
    for (int i = 0; i < 4; i++) f(p.threatPawnMg[i], true);
    for (int i = 0; i < 4; i++) f(p.threatPawnEg[i], true);
    for (int i = 0; i < 3; i++) f(p.kingShield[i], true);
    f(p.kingFileOpen, true);
    f(p.kingFileSemi, true);
    for (int i = 0; i < 4; i++) f(p.ksWeight[i], true);
    f(p.ksScale, true);
    f(p.tempo, true);
}

// PeSTO piece-square tables (Ronald Friederich, "Rofchade"), as published on the Chess Programming Wiki.
inline constexpr int PESTO_MAT_MG[6] = {82, 337, 365, 477, 1025, 0};
inline constexpr int PESTO_MAT_EG[6] = {94, 281, 297, 512, 936, 0};

inline constexpr int PESTO_MG[6][64] = {
    // pawn
    {0,   0,   0,   0,   0,   0,  0,   0,   98,  134, 61,  95,  68,  126, 34,  -11, -6,  7,   26,  31,  65,  56,
     25,  -20, -14, 13,  6,   21, 23,  12,  17,  -23, -27, -2,  -5,  12,  17,  6,   10,  -25, -26, -4,  -4,  -10,
     3,   3,   33,  -12, -35, -1, -20, -23, -15, 24,  38,  -22, 0,   0,   0,   0,   0,   0,   0,   0},
    // knight
    {-167, -89, -34, -49, 61,  -97, -15, -107, -73, -41, 72,  36,  23,  62,  7,   -17, -47, 60,  37,  65,  84,  129,
     73,   44,  -9,  17,  19,  53,  37,  69,   18,  22,  -13, 4,   16,  13,  28,  19,  21,  -8,  -23, -9,  12,  10,
     19,   17,  25,  -16, -29, -53, -12, -3,   -1,  18,  -14, -19, -105, -21, -58, -33, -17, -28, -19, -23},
    // bishop
    {-29, 4,   -82, -37, -25, -42, 7,   -8,  -26, 16, -18, -13, 30,  59,  18,  -47, -16, 37,  43,  40,  35,  50,
     37,  -2,  -4,  5,   19,  50,  37,  37,  7,   -2, -6,  13,  13,  26,  34,  12,  10,  4,   0,   15,  15,  15,
     14,  27,  18,  10,  4,   15,  16,  0,   7,   21, 33,  1,   -33, -3,  -14, -21, -13, -12, -39, -21},
    // rook
    {32,  42,  32,  51,  63,  9,   31,  43,  27,  32,  58,  62,  80,  67,  26,  44,  -5,  19,  26,  36,  17,  45,
     61,  16,  -24, -11, 7,   26,  24,  35,  -8,  -20, -36, -26, -12, -1,  9,   -7,  6,   -23, -45, -25, -16, -17,
     3,   0,   -5,  -33, -44, -16, -20, -9,  -1,  11,  -6,  -71, -19, -13, 1,   17,  16,  7,   -37, -26},
    // queen
    {-28, 0,   29,  12,  59,  44,  43,  45,  -24, -39, -5,  1,   -16, 57,  28,  54,  -13, -17, 7,   8,   29,  56,
     47,  57,  -27, -27, -16, -16, -1,  17,  -2,  1,   -9,  -26, -9,  -10, -2,  -4,  3,   -3,  -14, 2,   -11, -2,
     -5,  2,   14,  5,   -35, -8,  11,  2,   8,   15,  -3,  1,   -1,  -18, -9,  10,  -15, -25, -31, -50},
    // king
    {-65, 23,  16,  -15, -56, -34, 2,   13,  29,  -1,  -20, -7,  -8,  -4,  -38, -29, -9,  24,  2,   -16, -20, 6,
     22,  -22, -17, -20, -12, -27, -30, -25, -14, -36, -49, -1,  -27, -39, -46, -44, -33, -51, -14, -14, -22, -46,
     -44, -30, -15, -27, 1,   7,   -8,  -64, -43, -16, 9,   8,   -15, 36,  12,  -54, 8,   -28, 24,  14},
};

inline constexpr int PESTO_EG[6][64] = {
    // pawn
    {0,   0,   0,   0,   0,   0,   0,   0,   178, 173, 158, 134, 147, 132, 165, 187, 94,  100, 85,  67,  56,  53,
     82,  84,  32,  24,  13,  5,   -2,  4,   17,  17,  13,  9,   -3,  -7,  -7,  -8,  3,   -1,  4,   7,   -6,  1,
     0,   -5,  -1,  -8,  13,  8,   8,   10,  13,  0,   2,   -7,  0,   0,   0,   0,   0,   0,   0,   0},
    // knight
    {-58, -38, -13, -28, -31, -27, -63, -99, -25, -8,  -25, -2,  -9,  -25, -24, -52, -24, -20, 10,  9,   -1,  -9,
     -19, -41, -17, 3,   22,  22,  22,  11,  8,   -18, -18, -6,  16,  25,  16,  17,  4,   -18, -23, -3,  -1,  15,
     10,  -3,  -20, -22, -42, -20, -10, -5,  -2,  -20, -23, -44, -29, -51, -23, -15, -22, -18, -50, -64},
    // bishop
    {-14, -21, -11, -8,  -7,  -9,  -17, -24, -8,  -4,  7,   -12, -3,  -13, -4,  -14, 2,   -8,  0,   -1,  -2,  6,
     0,   4,   -3,  9,   12,  9,   14,  10,  3,   2,   -6,  3,   13,  19,  7,   10,  -3,  -9,  -12, -3,  8,   10,
     13,  3,   -7,  -15, -14, -18, -7,  -1,  4,   -9,  -15, -27, -23, -9,  -23, -5,  -9,  -16, -5,  -17},
    // rook
    {13,  10,  18,  15,  12,  12,  8,   5,   11,  13,  13,  11,  -3,  3,   8,   3,   7,   7,   7,   5,   4,   -3,
     -5,  -3,  4,   3,   13,  1,   2,   1,   -1,  2,   3,   5,   8,   4,   -5,  -6,  -8,  -11, -4,  0,   -5,  -1,
     -7,  -12, -8,  -16, -6,  -6,  0,   2,   -9,  -9,  -11, -3,  -9,  2,   3,   -1,  -5,  -13, 4,   -20},
    // queen
    {-9,  22,  22,  27,  27,  19,  10,  20,  -17, 20,  32,  41,  58,  25,  30,  0,   -20, 6,   9,   49,  47,  35,
     19,  9,   3,   22,  24,  45,  57,  40,  57,  36,  -18, 28,  19,  47,  31,  34,  39,  23,  -16, -27, 15,  6,
     9,   17,  10,  5,   -22, -23, -30, -16, -16, -23, -36, -32, -33, -28, -22, -43, -5,  -32, -20, -41},
    // king
    {-74, -35, -18, -18, -11, 15,  4,   -17, -12, 17,  14,  17,  17,  38,  23,  11,  10,  17,  23,  15,  20,  45,
     44,  13,  -8,  22,  24,  27,  26,  33,  26,  3,   -18, -4,  21,  24,  27,  23,  9,   -11, -19, -3,  11,  21,
     23,  16,  7,   -9,  -27, -11, 4,   13,  14,  4,   -5,  -17, -53, -34, -21, -11, -28, -14, -24, -43},
};

inline void setDefaultParams(Params& p) {
    for (int t = 0; t < 6; t++) {
        p.matMg[t] = PESTO_MAT_MG[t];
        p.matEg[t] = PESTO_MAT_EG[t];
        for (int s = 0; s < 64; s++) {
            p.pstMg[t][s] = PESTO_MG[t][s];
            p.pstEg[t][s] = PESTO_EG[t][s];
        }
    }
    const int mobMg[4] = {4, 3, 2, 1}, mobEg[4] = {4, 3, 4, 2};
    const int passedMg[8] = {0, 0, 0, 5, 10, 20, 35, 0}, passedEg[8] = {0, 5, 10, 20, 35, 60, 100, 0};
    const int supMg[8] = {0, 0, 4, 5, 8, 12, 15, 0}, supEg[8] = {0, 0, 3, 4, 6, 10, 15, 0};
    const int phaMg[8] = {0, 0, 3, 5, 8, 12, 16, 0}, phaEg[8] = {0, 0, 2, 4, 6, 10, 14, 0};
    for (int i = 0; i < 4; i++) {
        p.mobMg[i] = mobMg[i];
        p.mobEg[i] = mobEg[i];
        p.threatPawnMg[i] = 30 + 5 * (i / 2) * 2 + (i == 3 ? 10 : 0);
        p.threatPawnEg[i] = 20 + 5 * (i / 2);
        p.ksWeight[i] = (i == 0 ? 2 : i == 1 ? 2 : i == 2 ? 3 : 5);
    }
    for (int r = 0; r < 8; r++) {
        p.passedMg[r] = passedMg[r];
        p.passedEg[r] = passedEg[r];
        p.supportedMg[r] = supMg[r];
        p.supportedEg[r] = supEg[r];
        p.phalanxMg[r] = phaMg[r];
        p.phalanxEg[r] = phaEg[r];
    }
    p.doubledMg = -8;
    p.doubledEg = -12;
    p.isolatedMg = -10;
    p.isolatedEg = -12;
    p.bishopPairMg = 30;
    p.bishopPairEg = 45;
    p.rookOpenMg = 20;
    p.rookOpenEg = 10;
    p.rookSemiMg = 10;
    p.rookSemiEg = 8;
    p.rook7thMg = 10;
    p.rook7thEg = 20;
    p.outpostMg = 15;
    p.outpostEg = 10;
    p.kingShield[0] = 12;
    p.kingShield[1] = 6;
    p.kingShield[2] = -12;
    p.kingFileOpen = 18;
    p.kingFileSemi = 10;
    p.ksScale = 8;
    p.tempo = 10;
}

inline Params P;

inline bool initParams() {
    setDefaultParams(P);
    int total = 0;
    forEachParam(P, [&](int&, bool) { total++; });
    if (TUNED_COUNT == total) {  // saved tuned values (generated by the tuner) override the defaults
        int i = 0;
        forEachParam(P, [&](int& v, bool) { v = TUNED[i++]; });
    }
    return true;
}
inline const bool paramsReady = initParams();

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

// Records which parameters contributed to an evaluation and with what weight. Only the tuner uses
// this: it lets the evaluation be written as a simple weighted sum so it can be optimised quickly.
struct Trace {
    std::vector<int> idx;     // parameter number (its position inside the Params struct)
    std::vector<float> coef;  // weight before tapering (+1 for White's benefit, -1 for Black's, times a count)
    std::vector<char> isMg;   // 1 = middlegame parameter, 0 = endgame parameter
    bool constant = false;    // evaluation is a fixed value (insufficient material)
    int phase = 0;
    float mult = 1;    // scale factor from the drawish-endgame rules
    float offset = 0;  // king-danger term (already tapered and scaled)
    int tempoSign = 0;
    void add(const int* param, int c, bool mg) {
        if (c == 0) return;
        idx.push_back(int(param - reinterpret_cast<const int*>(&P)));
        coef.push_back(float(c));
        isMg.push_back(mg ? 1 : 0);
    }
};

}  // namespace Eval

// The evaluation. With TRACE = false this is the normal fast version; with TRACE = true it also
// fills in `tr` (see above). Returns the score in centipawns from the side to move's point of view.
template <bool TRACE>
inline int evalImpl(const Position& pos, Eval::Trace* tr) {
    using namespace Eval;
    const Params& p = P;

    // Neither side can mate: bare kings, or a king plus a single minor piece.
    if (!(pos.byType[PAWN] | pos.byType[ROOK] | pos.byType[QUEEN]) &&
        popcount(pos.byType[KNIGHT] | pos.byType[BISHOP]) <= 1) {
        if constexpr (TRACE) tr->constant = true;
        return 0;
    }

    const U64 occ = pos.occupied();
    const U64 pawnAtt[2] = {pawnAttackBB(pos.pieces(WHITE, PAWN), WHITE), pawnAttackBB(pos.pieces(BLACK, PAWN), BLACK)};

    int mg = 0, eg = 0;  // White's point of view
    int phase = 0;
    int npm[2] = {0, 0};  // non-pawn material per side

    const int kingSq[2] = {pos.kingSquare(WHITE), pos.kingSquare(BLACK)};
    const U64 kingZone[2] = {kingAttacks[kingSq[WHITE]] | bit(kingSq[WHITE]),
                             kingAttacks[kingSq[BLACK]] | bit(kingSq[BLACK])};
    int attackUnits[2] = {0, 0};  // attack units that colour's pieces direct at the ENEMY king
    int attackerCount[2] = {0, 0};

    for (int c = WHITE; c <= BLACK; c++) {
        const int sign = (c == WHITE) ? 1 : -1;
        const int them = c ^ 1;
        const U64 ownPawns = pos.pieces(c, PAWN);
        const U64 enemyPawns = pos.pieces(them, PAWN);
        const U64 safe = ~(pos.byColor[c] | pawnAtt[them]);  // squares a piece can usefully go to
        int cmg = 0, ceg = 0;                                // this colour's score

        // addMg / addEg: add `mult` times a parameter to this colour's middlegame / endgame score
        auto addMg = [&](const int& param, int mult = 1) {
            cmg += mult * param;
            if constexpr (TRACE) tr->add(&param, sign * mult, true);
        };
        auto addEg = [&](const int& param, int mult = 1) {
            ceg += mult * param;
            if constexpr (TRACE) tr->add(&param, sign * mult, false);
        };

        // Pawns: material, placement, passed / supported / phalanx / isolated / doubled
        U64 bb = ownPawns;
        while (bb) {
            int sq = popLsb(bb);
            int idx = (c == WHITE) ? (sq ^ 56) : sq;
            int f = sq % 8;
            int rel = (c == WHITE) ? sq / 8 : 7 - sq / 8;
            addMg(p.matMg[PAWN]);
            addMg(p.pstMg[PAWN][idx]);
            addEg(p.matEg[PAWN]);
            addEg(p.pstEg[PAWN][idx]);
            if (!(TABLES.passed[c][sq] & enemyPawns)) {
                addMg(p.passedMg[rel]);
                addEg(p.passedEg[rel]);
            }
            if (!(ownPawns & TABLES.adjacent[f])) {
                addMg(p.isolatedMg);
                addEg(p.isolatedEg);
            }
            if (pawnAtt[c] & bit(sq)) {
                addMg(p.supportedMg[rel]);
                addEg(p.supportedEg[rel]);
            }
            U64 beside = ((bit(sq) << 1) & ~FILE_A) | ((bit(sq) >> 1) & ~FILE_H);
            if (beside & ownPawns) {
                addMg(p.phalanxMg[rel]);
                addEg(p.phalanxEg[rel]);
            }
        }
        for (int f = 0; f < 8; f++) {
            int n = popcount(ownPawns & (FILE_A << f));
            if (n > 1) {
                addMg(p.doubledMg, n - 1);
                addEg(p.doubledEg, n - 1);
            }
        }

        // Knights, bishops, rooks, queens: material, placement, mobility, and piece-specific terms
        for (int t = KNIGHT; t <= QUEEN; t++) {
            bb = pos.pieces(c, t);
            const int count = popcount(bb);
            phase += count * PHASE_WEIGHT[t];
            npm[c] += count * NPM_VALUE[t];
            while (bb) {
                int sq = popLsb(bb);
                int idx = (c == WHITE) ? (sq ^ 56) : sq;
                int f = sq % 8;
                int rel = (c == WHITE) ? sq / 8 : 7 - sq / 8;
                U64 att;
                switch (t) {
                    case KNIGHT: att = knightAttacks[sq]; break;
                    case BISHOP: att = bishopAttacks(sq, occ); break;
                    case ROOK: att = rookAttacks(sq, occ); break;
                    default: att = queenAttacks(sq, occ); break;
                }
                int m = t - KNIGHT;  // 0..3
                int mob = popcount(att & safe) - MOB_BASE[m];
                addMg(p.matMg[t]);
                addMg(p.pstMg[t][idx]);
                addMg(p.mobMg[m], mob);
                addEg(p.matEg[t]);
                addEg(p.pstEg[t][idx]);
                addEg(p.mobEg[m], mob);
                U64 hit = att & kingZone[them];
                if (hit) {
                    attackerCount[c]++;
                    attackUnits[c] += p.ksWeight[m] * popcount(hit);
                }
                if (t == ROOK) {
                    U64 file = FILE_A << f;
                    if (!(file & (ownPawns | enemyPawns))) {
                        addMg(p.rookOpenMg);
                        addEg(p.rookOpenEg);
                    } else if (!(file & ownPawns)) {
                        addMg(p.rookSemiMg);
                        addEg(p.rookSemiEg);
                    }
                    if (rel == 6) {  // rook on the 7th rank, with something to attack there
                        int enemyKingRel = (c == WHITE) ? kingSq[them] / 8 : 7 - kingSq[them] / 8;
                        U64 seventh = 0xFFULL << (8 * (c == WHITE ? 6 : 1));
                        if (enemyKingRel == 7 || (enemyPawns & seventh)) {
                            addMg(p.rook7thMg);
                            addEg(p.rook7thEg);
                        }
                    }
                } else if (t == KNIGHT) {
                    if (rel >= 3 && rel <= 5 && (pawnAtt[c] & bit(sq)) &&
                        !(TABLES.passed[c][sq] & ~(FILE_A << f) & enemyPawns)) {
                        addMg(p.outpostMg);
                        addEg(p.outpostEg);
                    }
                }
            }
        }
        if (popcount(pos.pieces(c, BISHOP)) >= 2) {
            addMg(p.bishopPairMg);
            addEg(p.bishopPairEg);
        }

        // Our pawns attacking enemy pieces
        for (int t = KNIGHT; t <= QUEEN; t++) {
            int n = popcount(pawnAtt[c] & pos.pieces(them, t));
            addMg(p.threatPawnMg[t - KNIGHT], n);
            addEg(p.threatPawnEg[t - KNIGHT], n);
        }

        // King: placement, and the pawn shield in front of it (middlegame only)
        const int ksq = kingSq[c];
        const int kidx = (c == WHITE) ? (ksq ^ 56) : ksq;
        addMg(p.matMg[KING]);
        addMg(p.pstMg[KING][kidx]);
        addEg(p.matEg[KING]);
        addEg(p.pstEg[KING][kidx]);
        {
            const int kf = ksq % 8, kr = ksq / 8;
            const int krel = (c == WHITE) ? kr : 7 - kr;
            if (krel <= 1) {
                const int dir = (c == WHITE) ? 1 : -1;
                for (int ff = std::max(0, kf - 1); ff <= std::min(7, kf + 1); ff++) {
                    if (ownPawns & bit((kr + dir) * 8 + ff)) addMg(p.kingShield[0]);
                    else if (ownPawns & bit((kr + 2 * dir) * 8 + ff)) addMg(p.kingShield[1]);
                    else addMg(p.kingShield[2]);
                }
            }
        }

        // Open or half-open files next to the king matter when the enemy still has rooks or a queen.
        if (pos.pieces(them, ROOK) | pos.pieces(them, QUEEN)) {
            int kf = ksq % 8;
            for (int ff = std::max(0, kf - 1); ff <= std::min(7, kf + 1); ff++) {
                U64 file = FILE_A << ff;
                if (!(ownPawns & file)) {
                    if (enemyPawns & file) addMg(p.kingFileSemi, -1);
                    else addMg(p.kingFileOpen, -1);
                }
            }
        }

        mg += sign * cmg;
        eg += sign * ceg;
    }

    // King danger: the more (and the heavier) the enemy pieces bearing down on a king, the worse.
    // A lone attacker is far less dangerous than a coordinated group, so it counts for only a quarter.
    int dangerMg = 0, dangerEg = 0;
    for (int k = WHITE; k <= BLACK; k++) {
        int units = attackUnits[k ^ 1];
        int danger = std::min(units * units * p.ksScale / 256, KS_MAX);
        if (attackerCount[k ^ 1] < 2) danger /= 4;
        int sign = (k == WHITE) ? -1 : 1;
        dangerMg += sign * danger;
        dangerEg += sign * danger / 6;
    }
    mg += dangerMg;
    eg += dangerEg;

    phase = std::min(phase, TOTAL_PHASE);
    int score = (mg * phase + eg * (TOTAL_PHASE - phase)) / TOTAL_PHASE;

    // Drawish endings: a side with no pawns and only a small material edge can rarely win, and
    // bishops of opposite colours (with nothing else but pawns) are famously drawish.
    float mult = 1.0f;
    if (score != 0) {
        const int strong = score > 0 ? WHITE : BLACK;
        if (!pos.pieces(strong, PAWN) && npm[strong] - npm[strong ^ 1] < 400) {
            score /= 4;
            mult *= 0.25f;
        }
    }
    if (popcount(pos.pieces(WHITE, BISHOP)) == 1 && popcount(pos.pieces(BLACK, BISHOP)) == 1 &&
        !(pos.byType[KNIGHT] | pos.byType[ROOK] | pos.byType[QUEEN])) {
        int wsq = lsb(pos.pieces(WHITE, BISHOP)), bsq = lsb(pos.pieces(BLACK, BISHOP));
        if (((wsq % 8 + wsq / 8) & 1) != ((bsq % 8 + bsq / 8) & 1)) {
            score /= 2;
            mult *= 0.5f;
        }
    }

    // Mop-up: with a big material edge against a king that has no pawns and at most one minor piece,
    // the stronger side must actually deliver mate: drive the lone king to the edge, bring our king close.
    int mopUp = 0;
    {
        const int strong = npm[WHITE] > npm[BLACK] ? WHITE : BLACK, weak = strong ^ 1;
        if (npm[strong] - npm[weak] >= 400 && npm[weak] <= 330 && !pos.pieces(weak, PAWN)) {
            const int wk = kingSq[weak], sk = kingSq[strong];
            const int wf = wk % 8, wr = wk / 8;
            const int centreDist = std::max(3 - wf, wf - 4) + std::max(3 - wr, wr - 4);  // 0 (centre) .. 6 (corner)
            const int kingDist = std::max(std::abs(wf - sk % 8), std::abs(wr - sk / 8));  // 1 .. 7
            mopUp = 10 * centreDist + 6 * (7 - kingDist);
            if (strong == BLACK) mopUp = -mopUp;
        }
    }
    score += mopUp;

    if constexpr (TRACE) {
        tr->phase = phase;
        tr->mult = mult;
        tr->offset = mult * float(dangerMg * phase + dangerEg * (TOTAL_PHASE - phase)) / float(TOTAL_PHASE) + mopUp;
        tr->tempoSign = (pos.stm == WHITE) ? 1 : -1;
    }
    return (pos.stm == WHITE ? score : -score) + p.tempo;
}

// Returns the score in centipawns from the point of view of the side to move.
inline int evaluate(const Position& pos) { return evalImpl<false>(pos, nullptr); }
