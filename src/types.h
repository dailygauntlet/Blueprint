#pragma once
#include <bit>
#include <cstdint>
#include <string>

using U64 = uint64_t;

// Squares: a1 = 0, b1 = 1, ... h1 = 7, a2 = 8, ... h8 = 63
enum Color : int { WHITE, BLACK };
enum PieceType : int { PAWN, KNIGHT, BISHOP, ROOK, QUEEN, KING };

constexpr int NO_PIECE = 12;  // pieces are encoded as color * 6 + type

constexpr int makePiece(int c, int t) { return c * 6 + t; }
constexpr int typeOf(int p) { return p % 6; }
constexpr int colorOf(int p) { return p / 6; }

constexpr U64 bit(int sq) { return 1ULL << sq; }
inline int popcount(U64 b) { return std::popcount(b); }
inline int lsb(U64 b) { return std::countr_zero(b); }
inline int popLsb(U64& b) {
    int s = lsb(b);
    b &= b - 1;
    return s;
}

// Castling rights bits
enum : int { WK_CASTLE = 1, WQ_CASTLE = 2, BK_CASTLE = 4, BQ_CASTLE = 8 };

// Move: 16 bits = from(6) | to(6) << 6 | flags(4) << 12
using Move = uint16_t;
constexpr Move NULL_MOVE = 0;

enum MoveFlag : int {
    QUIET = 0,
    DOUBLE_PUSH = 1,
    KING_CASTLE = 2,
    QUEEN_CASTLE = 3,
    CAPTURE = 4,  // bit 2 set => capture
    EP_CAPTURE = 5,
    PROMO_N = 8,  // bit 3 set => promotion; (flags & 3) + 1 == promoted piece type
    PROMO_B = 9,
    PROMO_R = 10,
    PROMO_Q = 11,
};

constexpr Move makeMove(int from, int to, int flags) {
    return Move(from | (to << 6) | (flags << 12));
}
constexpr int moveFrom(Move m) { return m & 63; }
constexpr int moveTo(Move m) { return (m >> 6) & 63; }
constexpr int moveFlags(Move m) { return m >> 12; }
constexpr bool isCapture(Move m) { return moveFlags(m) & 4; }
constexpr bool isPromotion(Move m) { return moveFlags(m) & 8; }
constexpr int promoType(Move m) { return (moveFlags(m) & 3) + 1; }

inline std::string squareName(int sq) {
    return std::string{char('a' + sq % 8), char('1' + sq / 8)};
}

inline std::string moveToString(Move m) {
    std::string s = squareName(moveFrom(m)) + squareName(moveTo(m));
    if (isPromotion(m)) s += "nbrq"[promoType(m) - 1];
    return s;
}
