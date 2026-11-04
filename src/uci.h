#pragma once
#include <atomic>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>

#include "perft.h"
#include "search.h"

// Change these to whatever you like. They're what the GUI (Arena) and CCRL will display.
constexpr const char* ENGINE_NAME = "MyEngine";
constexpr const char* ENGINE_AUTHOR = "Your Name";
constexpr const char* ENGINE_VERSION = "0.1";

// UCI (Universal Chess Interface): the text protocol GUIs use to talk to engines.
// The GUI sends lines like "position startpos moves e2e4" and "go wtime 60000 btime 60000",
// and the engine replies with "info ..." lines while thinking and finally "bestmove e7e5".
class UCI {
public:
    ~UCI() { stopSearch(); }

    void loop() {
        std::string line;
        while (std::getline(std::cin, line)) {
            std::istringstream is(line);
            std::string cmd;
            is >> cmd;
            if (cmd == "uci") {
                std::cout << "id name " << ENGINE_NAME << " " << ENGINE_VERSION << "\n"
                          << "id author " << ENGINE_AUTHOR << "\n"
                          << "option name Hash type spin default 16 min 1 max 4096\n"
                          << "option name Move Overhead type spin default 30 min 0 max 5000\n"
                          << "uciok" << std::endl;
            } else if (cmd == "isready") {
                std::cout << "readyok" << std::endl;
            } else if (cmd == "ucinewgame") {
                stopSearch();
                tt.clear();
                pos.setFen(Position::START_FEN);
            } else if (cmd == "setoption") {
                setOption(is);
            } else if (cmd == "position") {
                stopSearch();
                setPosition(is);
            } else if (cmd == "go") {
                go(is);
            } else if (cmd == "stop") {
                stopSearch();
            } else if (cmd == "quit") {
                stopSearch();
                return;
            } else if (cmd == "perft" || cmd == "divide") {  // handy debugging commands
                int d = 1;
                is >> d;
                if (cmd == "perft") timedPerft(pos, d);
                else divide(pos, d);
            }
        }
    }

private:
    Position pos;
    TT tt;
    std::atomic<bool> stopFlag{false};
    std::thread worker;
    int moveOverhead = 30;

    void stopSearch() {
        stopFlag = true;
        if (worker.joinable()) worker.join();
    }

    void setOption(std::istringstream& is) {
        std::string tok, name, value;
        is >> tok;  // "name"
        while (is >> tok && tok != "value") name += (name.empty() ? "" : " ") + tok;
        while (is >> tok) value += (value.empty() ? "" : " ") + tok;
        stopSearch();
        if (name == "Hash") tt.resize(std::max(1, std::atoi(value.c_str())));
        else if (name == "Move Overhead") moveOverhead = std::max(0, std::atoi(value.c_str()));
    }

    // Finds the legal move matching a UCI string like "e2e4" or "e7e8q".
    static Move parseMove(Position& p, const std::string& s) {
        MoveList list;
        p.generate(list);
        for (int i = 0; i < list.count; i++) {
            if (moveToString(list.moves[i]) != s) continue;
            State st;
            p.makeMove(list.moves[i], st);
            bool ok = p.lastMoveLegal();
            p.unmakeMove(list.moves[i], st);
            if (ok) return list.moves[i];
        }
        return NULL_MOVE;
    }

    void setPosition(std::istringstream& is) {
        std::string tok;
        is >> tok;
        if (tok == "startpos") {
            pos.setFen(Position::START_FEN);
            is >> tok;
        } else if (tok == "fen") {
            std::string fen;
            while (is >> tok && tok != "moves") fen += tok + " ";
            if (!pos.setFen(fen)) {
                std::cout << "info string bad FEN" << std::endl;
                return;
            }
        }
        if (tok == "moves") {
            std::string mv;
            while (is >> mv) {
                Move m = parseMove(pos, mv);
                if (m == NULL_MOVE) {
                    std::cout << "info string illegal move " << mv << std::endl;
                    break;
                }
                State st;
                pos.makeMove(m, st);
            }
        }
    }

    void go(std::istringstream& is) {
        stopSearch();
        int64_t wtime = -1, btime = -1, winc = 0, binc = 0, movetime = -1;
        int movestogo = 0;
        SearchLimits lim;
        std::string tok;
        while (is >> tok) {
            if (tok == "wtime") is >> wtime;
            else if (tok == "btime") is >> btime;
            else if (tok == "winc") is >> winc;
            else if (tok == "binc") is >> binc;
            else if (tok == "movestogo") is >> movestogo;
            else if (tok == "movetime") is >> movetime;
            else if (tok == "depth") is >> lim.maxDepth;
            else if (tok == "nodes") is >> lim.maxNodes;
        }

        // Time management: decide how long to spend on this move.
        const int64_t myTime = (pos.stm == WHITE) ? wtime : btime;
        const int64_t myInc = (pos.stm == WHITE) ? winc : binc;
        if (movetime > 0) {
            lim.softMs = lim.hardMs = std::max<int64_t>(1, movetime - moveOverhead);
        } else if (myTime >= 0) {
            int64_t avail = std::max<int64_t>(1, myTime - moveOverhead);
            int64_t mtg = movestogo > 0 ? std::min(movestogo, 50) : 30;
            int64_t soft = avail / mtg + myInc * 3 / 4;
            soft = std::min(soft, avail * 8 / 10);
            lim.softMs = std::max<int64_t>(1, soft);
            lim.hardMs = std::max<int64_t>(1, std::min(soft * 4, avail * 9 / 10));
        }

        stopFlag = false;
        Position copy = pos;  // the search thread works on its own copy
        worker = std::thread([this, copy, lim]() mutable {
            Searcher searcher(tt, stopFlag);
            Move best = searcher.search(copy, lim);
            std::cout << "bestmove " << (best == NULL_MOVE ? "0000" : moveToString(best)) << std::endl;
        });
    }
};
