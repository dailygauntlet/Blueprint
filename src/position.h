#pragma once
#include <sstream>
#include <string>

#include "attacks.h"
#include "types.h"

// Size of the game-history buffer used for repetition detection (tools that only evaluate positions shrink it).
#ifndef BP_MAX_HIST
#define BP_MAX_HIST 2048
#endif

struct MoveList {
    Move moves[256];
    int count = 0;
    void add(int from, int to, int flags) { moves[count++] = makeMove(from, to, flags); }
};

// Information needed to undo a move.
struct State {
    int castling;
    int ep;
    int halfmove;
    int captured;
    U64 key;
};

// Zobrist hashing: a random number per (piece, square), castling state, en-passant file and side to move.
// XOR-ing them together gives every position a (nearly) unique 64-bit fingerprint.
namespace Zobrist {
inline U64 piece[12][64];
inline U64 castle[16];
inline U64 epFile[8];
inline U64 side;

inline void init() {
    for (auto& row : piece)
        for (auto& k : row) k = attacks_detail::rng();
    for (auto& k : castle) k = attacks_detail::rng();
    for (auto& k : epFile) k = attacks_detail::rng();
    side = attacks_detail::rng();
}
}  // namespace Zobrist

inline void initEngine() {
    Attacks::init();
    Zobrist::init();
}

struct Position {
    U64 byType[6] = {};
    U64 byColor[2] = {};
    int board[64];
    int stm = WHITE;
    int castling = 0;
    int ep = -1;  // en-passant target square, or -1
    int halfmove = 0;
    int fullmove = 1;
    U64 key = 0;

    // Keys of every earlier position in the game (and search), used to detect repetitions.
    static constexpr int MAX_HIST = BP_MAX_HIST;
    U64 hist[MAX_HIST];
    int histLen = 0;

    static constexpr const char* START_FEN = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";

    Position() { setFen(START_FEN); }

    U64 occupied() const { return byColor[WHITE] | byColor[BLACK]; }
    U64 pieces(int c, int t) const { return byType[t] & byColor[c]; }
    int kingSquare(int c) const { return lsb(pieces(c, KING)); }

    // ---- piece placement helpers ----
    void putPiece(int pc, int sq) {
        board[sq] = pc;
        key ^= Zobrist::piece[pc][sq];
        byType[typeOf(pc)] |= bit(sq);
        byColor[colorOf(pc)] |= bit(sq);
    }
    void removePiece(int sq) {
        int pc = board[sq];
        key ^= Zobrist::piece[pc][sq];
        byType[typeOf(pc)] &= ~bit(sq);
        byColor[colorOf(pc)] &= ~bit(sq);
        board[sq] = NO_PIECE;
    }
    void movePiece(int from, int to) {
        int pc = board[from];
        removePiece(from);
        putPiece(pc, to);
    }

    // ---- FEN ----
    bool setFen(const std::string& fen) {
        for (int i = 0; i < 64; i++) board[i] = NO_PIECE;
        byType[0] = byType[1] = byType[2] = byType[3] = byType[4] = byType[5] = 0;
        byColor[0] = byColor[1] = 0;
        key = 0;
        histLen = 0;

        std::istringstream ss(fen);
        std::string placement, side, castle, epStr;
        if (!(ss >> placement >> side >> castle >> epStr)) return false;
        halfmove = 0;
        fullmove = 1;
        ss >> halfmove >> fullmove;

        int rank = 7, file = 0;
        for (char ch : placement) {
            if (ch == '/') {
                rank--;
                file = 0;
            } else if (ch >= '1' && ch <= '8') {
                file += ch - '0';
            } else {
                const std::string order = "PNBRQKpnbrqk";
                size_t idx = order.find(ch);
                if (idx == std::string::npos || rank < 0 || file > 7) return false;
                putPiece(int(idx), rank * 8 + file);  // order matches color*6+type
                file++;
            }
        }
        stm = (side == "w") ? WHITE : BLACK;
        castling = 0;
        for (char ch : castle) {
            if (ch == 'K') castling |= WK_CASTLE;
            if (ch == 'Q') castling |= WQ_CASTLE;
            if (ch == 'k') castling |= BK_CASTLE;
            if (ch == 'q') castling |= BQ_CASTLE;
        }
        ep = -1;
        if (epStr != "-" && epStr.size() == 2) ep = (epStr[1] - '1') * 8 + (epStr[0] - 'a');
        key ^= Zobrist::castle[castling];
        if (ep >= 0) key ^= Zobrist::epFile[ep & 7];
        if (stm == BLACK) key ^= Zobrist::side;
        return true;
    }

    std::string fen() const {
        std::string s;
        for (int r = 7; r >= 0; r--) {
            int empty = 0;
            for (int f = 0; f < 8; f++) {
                int pc = board[r * 8 + f];
                if (pc == NO_PIECE) {
                    empty++;
                } else {
                    if (empty) s += char('0' + empty);
                    empty = 0;
                    s += "PNBRQKpnbrqk"[pc];
                }
            }
            if (empty) s += char('0' + empty);
            if (r) s += '/';
        }
        s += (stm == WHITE) ? " w " : " b ";
        std::string c;
        if (castling & WK_CASTLE) c += 'K';
        if (castling & WQ_CASTLE) c += 'Q';
        if (castling & BK_CASTLE) c += 'k';
        if (castling & BQ_CASTLE) c += 'q';
        s += c.empty() ? "-" : c;
        s += ' ';
        s += (ep >= 0) ? squareName(ep) : std::string("-");
        s += ' ' + std::to_string(halfmove) + ' ' + std::to_string(fullmove);
        return s;
    }

    // ---- attack detection ----
    bool isAttacked(int sq, int by) const {
        U64 occ = occupied();
        if (pawnAttacks[by ^ 1][sq] & pieces(by, PAWN)) return true;
        if (knightAttacks[sq] & pieces(by, KNIGHT)) return true;
        if (kingAttacks[sq] & pieces(by, KING)) return true;
        if (bishopAttacks(sq, occ) & (pieces(by, BISHOP) | pieces(by, QUEEN))) return true;
        if (rookAttacks(sq, occ) & (pieces(by, ROOK) | pieces(by, QUEEN))) return true;
        return false;
    }
    bool inCheck() const { return isAttacked(kingSquare(stm), stm ^ 1); }

    // After makeMove: true if the side that just moved left its own king safe.
    bool lastMoveLegal() const { return !isAttacked(kingSquare(stm ^ 1), stm); }

    // ---- make / unmake ----
    void makeMove(Move m, State& st) {
        st = {castling, ep, halfmove, NO_PIECE, key};
        hist[histLen++] = key;
        key ^= Zobrist::castle[castling];
        if (ep >= 0) key ^= Zobrist::epFile[ep & 7];
        int from = moveFrom(m), to = moveTo(m), flags = moveFlags(m);
        int pc = board[from];
        int us = stm;

        ep = -1;
        halfmove++;
        if (typeOf(pc) == PAWN) halfmove = 0;

        if (flags == EP_CAPTURE) {
            int capSq = to + (us == WHITE ? -8 : 8);
            st.captured = board[capSq];
            removePiece(capSq);
            halfmove = 0;
        } else if (isCapture(m)) {
            st.captured = board[to];
            removePiece(to);
            halfmove = 0;
        }

        if (isPromotion(m)) {
            removePiece(from);
            putPiece(makePiece(us, promoType(m)), to);
        } else {
            movePiece(from, to);
        }

        if (flags == KING_CASTLE) movePiece(to + 1, to - 1);
        else if (flags == QUEEN_CASTLE) movePiece(to - 2, to + 1);
        else if (flags == DOUBLE_PUSH) ep = from + (us == WHITE ? 8 : -8);

        castling &= castleKeep(from) & castleKeep(to);
        key ^= Zobrist::castle[castling];
        if (ep >= 0) key ^= Zobrist::epFile[ep & 7];
        key ^= Zobrist::side;
        stm ^= 1;
        if (us == BLACK) fullmove++;
    }

    void unmakeMove(Move m, const State& st) {
        int from = moveFrom(m), to = moveTo(m), flags = moveFlags(m);
        stm ^= 1;
        int us = stm;
        if (us == BLACK) fullmove--;
        castling = st.castling;
        ep = st.ep;
        halfmove = st.halfmove;

        if (isPromotion(m)) {
            removePiece(to);
            putPiece(makePiece(us, PAWN), from);
        } else {
            movePiece(to, from);
        }

        if (flags == KING_CASTLE) movePiece(to - 1, to + 1);
        else if (flags == QUEEN_CASTLE) movePiece(to + 1, to - 2);

        if (flags == EP_CAPTURE) putPiece(st.captured, to + (us == WHITE ? -8 : 8));
        else if (isCapture(m)) putPiece(st.captured, to);

        key = st.key;
        histLen--;
    }

    // "Null move": pass the turn without moving. Only used by the search for pruning.
    void makeNullMove(State& st) {
        st = {castling, ep, halfmove, NO_PIECE, key};
        hist[histLen++] = key;
        if (ep >= 0) key ^= Zobrist::epFile[ep & 7];
        ep = -1;
        halfmove++;
        key ^= Zobrist::side;
        stm ^= 1;
    }
    void unmakeNullMove(const State& st) {
        stm ^= 1;
        ep = st.ep;
        halfmove = st.halfmove;
        key = st.key;
        histLen--;
    }

    // True if the current position already occurred earlier (only positions since the last
    // capture or pawn move can match, so we only look back `halfmove` plies).
    bool isRepetition() const {
        int limit = histLen - halfmove;
        if (limit < 0) limit = 0;
        for (int i = histLen - 2; i >= limit; i -= 2)
            if (hist[i] == key) return true;
        return false;
    }

    // ---- move generation (pseudo-legal; filter with lastMoveLegal after makeMove) ----
    void generate(MoveList& list) const {
        const int us = stm, them = us ^ 1;
        const U64 occ = occupied();
        const U64 own = byColor[us], enemy = byColor[them];

        // Pawns
        const int dir = us == WHITE ? 8 : -8;
        const int startRank = us == WHITE ? 1 : 6;
        const int lastRankBefore = us == WHITE ? 6 : 1;  // pawn about to promote
        U64 pawns = pieces(us, PAWN);
        while (pawns) {
            int sq = popLsb(pawns);
            int to = sq + dir;
            if (!(occ & bit(to))) {
                if (sq / 8 == lastRankBefore) {
                    list.add(sq, to, PROMO_Q);
                    list.add(sq, to, PROMO_R);
                    list.add(sq, to, PROMO_B);
                    list.add(sq, to, PROMO_N);
                } else {
                    list.add(sq, to, QUIET);
                    if (sq / 8 == startRank && !(occ & bit(to + dir))) list.add(sq, to + dir, DOUBLE_PUSH);
                }
            }
            U64 caps = pawnAttacks[us][sq] & enemy;
            while (caps) {
                int t = popLsb(caps);
                if (sq / 8 == lastRankBefore) {
                    list.add(sq, t, PROMO_Q | CAPTURE);
                    list.add(sq, t, PROMO_R | CAPTURE);
                    list.add(sq, t, PROMO_B | CAPTURE);
                    list.add(sq, t, PROMO_N | CAPTURE);
                } else {
                    list.add(sq, t, CAPTURE);
                }
            }
            if (ep >= 0 && (pawnAttacks[us][sq] & bit(ep))) list.add(sq, ep, EP_CAPTURE);
        }

        // Knights, bishops, rooks, queens, king
        for (int t = KNIGHT; t <= KING; t++) {
            U64 bb = pieces(us, t);
            while (bb) {
                int sq = popLsb(bb);
                U64 att;
                switch (t) {
                    case KNIGHT: att = knightAttacks[sq]; break;
                    case BISHOP: att = bishopAttacks(sq, occ); break;
                    case ROOK: att = rookAttacks(sq, occ); break;
                    case QUEEN: att = queenAttacks(sq, occ); break;
                    default: att = kingAttacks[sq]; break;
                }
                att &= ~own;
                while (att) {
                    int to = popLsb(att);
                    list.add(sq, to, (enemy & bit(to)) ? CAPTURE : QUIET);
                }
            }
        }

        // Castling
        if (us == WHITE) {
            if ((castling & WK_CASTLE) && !(occ & (bit(5) | bit(6))) && !isAttacked(4, them) &&
                !isAttacked(5, them) && !isAttacked(6, them))
                list.add(4, 6, KING_CASTLE);
            if ((castling & WQ_CASTLE) && !(occ & (bit(1) | bit(2) | bit(3))) && !isAttacked(4, them) &&
                !isAttacked(3, them) && !isAttacked(2, them))
                list.add(4, 2, QUEEN_CASTLE);
        } else {
            if ((castling & BK_CASTLE) && !(occ & (bit(61) | bit(62))) && !isAttacked(60, them) &&
                !isAttacked(61, them) && !isAttacked(62, them))
                list.add(60, 62, KING_CASTLE);
            if ((castling & BQ_CASTLE) && !(occ & (bit(57) | bit(58) | bit(59))) && !isAttacked(60, them) &&
                !isAttacked(59, them) && !isAttacked(58, them))
                list.add(60, 58, QUEEN_CASTLE);
        }
    }

private:
    // Which castling rights survive a move touching this square.
    static int castleKeep(int sq) {
        switch (sq) {
            case 0: return 15 & ~WQ_CASTLE;
            case 7: return 15 & ~WK_CASTLE;
            case 4: return 15 & ~(WK_CASTLE | WQ_CASTLE);
            case 56: return 15 & ~BQ_CASTLE;
            case 63: return 15 & ~BK_CASTLE;
            case 60: return 15 & ~(BK_CASTLE | BQ_CASTLE);
            default: return 15;
        }
    }
};
