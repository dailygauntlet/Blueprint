#pragma once
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <iostream>

#include "eval.h"
#include "tt.h"

constexpr int MAX_PLY = 128;
constexpr int MATE = 30000;
constexpr int INF = 32000;
constexpr int MATE_BOUND = MATE - MAX_PLY;  // scores beyond this mean "forced mate"

struct SearchLimits {
    int64_t softMs = 0;  // don't start a new depth after roughly this long (0 = no limit)
    int64_t hardMs = 0;  // abort immediately after this long (0 = no limit)
    int maxDepth = 0;    // 0 = no limit
    uint64_t maxNodes = 0;
};

class Searcher {
public:
    Searcher(TT& table, std::atomic<bool>& stopFlag) : tt(table), stop(stopFlag) {}

    // Searches the position and returns the best move found (NULL_MOVE if there are no legal moves).
    Move search(Position& pos, const SearchLimits& limits) {
        lim = limits;
        start = std::chrono::steady_clock::now();
        nodes = 0;
        stopped = false;
        completedDepth = 0;
        tt.newSearch();

        Move best = NULL_MOVE;
        const int maxDepth = lim.maxDepth > 0 ? std::min(lim.maxDepth, MAX_PLY - 1) : MAX_PLY - 1;

        // Iterative deepening: search depth 1, then 2, then 3, ... Each pass is cheap compared to the
        // next, and the earlier results make move ordering much better in the later ones.
        for (int depth = 1; depth <= maxDepth; depth++) {
            int score = negamax(pos, depth, -INF, INF, 0);
            if (stopped) break;  // this depth was cut short: throw it away, keep the previous result
            completedDepth = depth;
            best = pv[0][0];
            printInfo(depth, score);
            if (lim.softMs && elapsedMs() >= lim.softMs / 2) break;
        }

        if (best == NULL_MOVE) best = firstLegalMove(pos);
        return best;
    }

private:
    TT& tt;
    std::atomic<bool>& stop;
    SearchLimits lim;
    std::chrono::steady_clock::time_point start;
    uint64_t nodes = 0;
    bool stopped = false;
    int completedDepth = 0;

    Move pv[MAX_PLY + 2][MAX_PLY + 2];  // triangular principal-variation table
    int pvLen[MAX_PLY + 2];
    Move killers[MAX_PLY + 2][2] = {};
    int history[2][64][64] = {};

    int64_t elapsedMs() const {
        return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count();
    }

    void checkTime() {
        if (completedDepth < 1) return;  // always finish depth 1 so we have a move to play
        if (stop.load(std::memory_order_relaxed)) stopped = true;
        else if (lim.hardMs && elapsedMs() >= lim.hardMs) stopped = true;
        else if (lim.maxNodes && nodes >= lim.maxNodes) stopped = true;
    }

    static Move firstLegalMove(Position& pos) {
        MoveList list;
        pos.generate(list);
        for (int i = 0; i < list.count; i++) {
            State st;
            pos.makeMove(list.moves[i], st);
            bool ok = pos.lastMoveLegal();
            pos.unmakeMove(list.moves[i], st);
            if (ok) return list.moves[i];
        }
        return NULL_MOVE;
    }

    void printInfo(int depth, int score) {
        int64_t ms = elapsedMs();
        uint64_t nps = ms > 0 ? nodes * 1000 / ms : 0;
        std::cout << "info depth " << depth << " score ";
        if (std::abs(score) > MATE_BOUND) {
            int n = (MATE - std::abs(score) + 1) / 2;
            std::cout << "mate " << (score > 0 ? n : -n);
        } else {
            std::cout << "cp " << score;
        }
        std::cout << " nodes " << nodes << " nps " << nps << " time " << ms << " pv";
        for (int i = 0; i < pvLen[0]; i++) std::cout << ' ' << moveToString(pv[0][i]);
        std::cout << std::endl;
    }

    // Mate scores are stored relative to the root ("mate in N from here"), but the table must store
    // them relative to the position, since the same position can be reached at different plies.
    static int scoreToTT(int s, int ply) {
        if (s > MATE_BOUND) return s + ply;
        if (s < -MATE_BOUND) return s - ply;
        return s;
    }
    static int scoreFromTT(int s, int ply) {
        if (s > MATE_BOUND) return s - ply;
        if (s < -MATE_BOUND) return s + ply;
        return s;
    }

    // ---- move ordering: try the likely-best moves first so alpha-beta can cut off sooner ----
    int scoreMove(const Position& pos, Move m, Move ttMove, int ply) const {
        static const int VICTIM[6] = {1, 3, 3, 5, 9, 0};
        if (m == ttMove) return 10000000;
        int flags = moveFlags(m);
        if (isCapture(m) || isPromotion(m)) {
            int s = 200000;
            if (isCapture(m)) {
                int victim = flags == EP_CAPTURE ? PAWN : typeOf(pos.board[moveTo(m)]);
                // MVV-LVA: most valuable victim first, then least valuable attacker
                s += VICTIM[victim] * 16 - typeOf(pos.board[moveFrom(m)]);
            }
            if (isPromotion(m)) s += (promoType(m) == QUEEN) ? 1500 : -300000;  // underpromotions last
            return s;
        }
        if (m == killers[ply][0]) return 150000;
        if (m == killers[ply][1]) return 140000;
        return history[pos.stm][moveFrom(m)][moveTo(m)];
    }

    void updateHistory(int color, Move m, int bonus) {
        int& h = history[color][moveFrom(m)][moveTo(m)];
        h += bonus - h * std::abs(bonus) / 16384;
    }

    // A quiet move caused a beta cutoff: remember it (killer + history), and penalise the quiet
    // moves we tried before it that did not work.
    void rewardQuiet(int color, Move best, const Move* quiets, int nq, int depth, int ply) {
        if (killers[ply][0] != best) {
            killers[ply][1] = killers[ply][0];
            killers[ply][0] = best;
        }
        int bonus = std::min(depth * depth, 1200);
        updateHistory(color, best, bonus);
        for (int i = 0; i < nq; i++) updateHistory(color, quiets[i], -bonus);
    }

    // ---- quiescence search: keep playing captures until the position is "quiet" ----
    // Without this, the engine would stop mid-exchange and think it just won a queen for free.
    int quiesce(Position& pos, int alpha, int beta, int ply) {
        pvLen[ply] = ply;
        if (stopped) return 0;
        if ((++nodes & 2047) == 0) checkTime();
        if (ply >= MAX_PLY - 1) return evaluate(pos);

        const bool inCheck = pos.inCheck();
        int best = -INF;
        if (!inCheck) {
            int stand = evaluate(pos);  // "stand pat": we may always decline to capture anything
            if (stand >= beta) return stand;
            if (stand > alpha) alpha = stand;
            best = stand;
        }

        MoveList all;
        pos.generate(all);
        Move moves[256];
        int scores[256];
        int n = 0;
        for (int i = 0; i < all.count; i++) {
            Move m = all.moves[i];
            if (!inCheck && !isCapture(m) && moveFlags(m) != PROMO_Q) continue;
            moves[n] = m;
            scores[n] = scoreMove(pos, m, NULL_MOVE, ply);
            n++;
        }

        int legal = 0;
        for (int i = 0; i < n; i++) {
            int bi = i;
            for (int j = i + 1; j < n; j++)
                if (scores[j] > scores[bi]) bi = j;
            std::swap(moves[i], moves[bi]);
            std::swap(scores[i], scores[bi]);

            State st;
            pos.makeMove(moves[i], st);
            if (!pos.lastMoveLegal()) {
                pos.unmakeMove(moves[i], st);
                continue;
            }
            legal++;
            int score = -quiesce(pos, -beta, -alpha, ply + 1);
            pos.unmakeMove(moves[i], st);
            if (stopped) return 0;

            if (score > best) {
                best = score;
                if (score > alpha) {
                    alpha = score;
                    if (alpha >= beta) break;
                }
            }
        }
        if (inCheck && legal == 0) return -MATE + ply;  // checkmated
        return best;
    }

    // ---- main search (negamax form of alpha-beta with principal variation search) ----
    // Returns the score from the point of view of the side to move.
    int negamax(Position& pos, int depth, int alpha, int beta, int ply) {
        pvLen[ply] = ply;
        if (stopped) return 0;

        const bool root = (ply == 0);
        const bool pvNode = (beta - alpha > 1);

        if (!root) {
            if (pos.halfmove >= 100 || pos.isRepetition()) return 0;  // draw by 50-move rule / repetition
            if (ply >= MAX_PLY - 1) return evaluate(pos);
            // Mate distance pruning: we can't do better than mating right now, or worse than being mated now.
            alpha = std::max(alpha, -MATE + ply);
            beta = std::min(beta, MATE - ply - 1);
            if (alpha >= beta) return alpha;
        }

        const bool inCheck = pos.inCheck();
        if (inCheck) depth++;  // check extension: never stop searching while in check
        if (depth <= 0) return quiesce(pos, alpha, beta, ply);

        if ((++nodes & 2047) == 0) checkTime();

        // Transposition table lookup
        Move ttMove = NULL_MOVE;
        TTEntry* e = tt.probe(pos.key);
        if (e->key == pos.key) {
            ttMove = e->move;
            if (!pvNode && e->depth >= depth) {
                int s = scoreFromTT(e->score, ply);
                if (e->flag == TT_EXACT || (e->flag == TT_LOWER && s >= beta) || (e->flag == TT_UPPER && s <= alpha))
                    return s;
            }
        }

        MoveList list;
        pos.generate(list);
        int scores[256];
        for (int i = 0; i < list.count; i++) scores[i] = scoreMove(pos, list.moves[i], ttMove, ply);

        const int origAlpha = alpha;
        int best = -INF;
        Move bestMove = NULL_MOVE;
        int legal = 0;
        Move quiets[64];
        int nq = 0;

        for (int i = 0; i < list.count; i++) {
            // pick the highest-scored remaining move
            int bi = i;
            for (int j = i + 1; j < list.count; j++)
                if (scores[j] > scores[bi]) bi = j;
            std::swap(list.moves[i], list.moves[bi]);
            std::swap(scores[i], scores[bi]);
            const Move m = list.moves[i];

            State st;
            pos.makeMove(m, st);
            if (!pos.lastMoveLegal()) {
                pos.unmakeMove(m, st);
                continue;
            }
            legal++;

            int score;
            if (legal == 1) {
                score = -negamax(pos, depth - 1, -beta, -alpha, ply + 1);
            } else {
                // PVS: assume this move is worse than the first; check with a cheap zero-width window,
                // and only do the full search if it turns out to be better.
                score = -negamax(pos, depth - 1, -alpha - 1, -alpha, ply + 1);
                if (score > alpha && score < beta) score = -negamax(pos, depth - 1, -beta, -alpha, ply + 1);
            }
            pos.unmakeMove(m, st);
            if (stopped) return 0;

            if (score > best) {
                best = score;
                bestMove = m;
                if (score > alpha) {
                    alpha = score;
                    pv[ply][ply] = m;
                    for (int j = ply + 1; j < pvLen[ply + 1]; j++) pv[ply][j] = pv[ply + 1][j];
                    pvLen[ply] = pvLen[ply + 1];
                    if (alpha >= beta) {  // beta cutoff: the opponent would never allow this position
                        if (!isCapture(m) && !isPromotion(m)) rewardQuiet(pos.stm, m, quiets, nq, depth, ply);
                        break;
                    }
                }
            }
            if (!isCapture(m) && !isPromotion(m) && nq < 64) quiets[nq++] = m;
        }

        if (legal == 0) return inCheck ? -MATE + ply : 0;  // checkmate or stalemate

        int flag = best >= beta ? TT_LOWER : (best > origAlpha ? TT_EXACT : TT_UPPER);
        tt.store(pos.key, bestMove, scoreToTT(best, ply), depth, flag);
        return best;
    }
};
