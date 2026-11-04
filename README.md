# Blueprint
<img width="1254" height="1254" alt="image" src="https://github.com/user-attachments/assets/79d9a5c3-cce8-46b1-919a-d2ccd5d773ba" />

A UCI chess engine written in C++20.



- **Author:** Everett Towe (United States)
- **Version:** 0.3
- **License:** MIT (see `LICENSE`)
- **Development:** written with AI assistance (Claude, by Anthropic), directed and tested by the author.
- **Protocol:** UCI. It needs a GUI such as Arena, Cute Chess or En Croissant.
- **Threads:** single-threaded.

## Features

- Bitboards with magic bitboard move generation (perft-verified)
- Iterative deepening, principal variation search, aspiration windows
- Quiescence search, check extensions, transposition table (Zobrist hashing)
- Null-move pruning, reverse futility pruning, late move reductions
- Move ordering: hash move, MVV-LVA, killer moves, history heuristic
- Evaluation (tapered middlegame/endgame): material, piece-square tables, piece mobility, passed / doubled /
  isolated pawns, bishop pair, rooks on open and half-open files, king pawn shield
- Draw detection: repetition, 50-move rule, insufficient material

## UCI options

| Option | Default | Description |
|---|---|---|
| Hash | 256 | Transposition table size in MB |
| Move Overhead | 30 | Milliseconds reserved per move for GUI/network lag |

## Building

    g++ -std=c++20 -O3 -pthread -static -march=x86-64-v3 src/main.cpp -o blueprint.exe

Use `-march=x86-64-v2` instead for older CPUs without AVX2. Or run `make`.

Developer commands: `blueprint.exe test 5` (perft suite), `blueprint.exe perft 6`.

## Credits

- Piece-square table values come from Tomasz Michniewski's "Simplified Evaluation Function".
- Search techniques and the magic bitboard approach follow the descriptions on the
  Chess Programming Wiki (chessprogramming.org).
- Perft test positions are the standard ones published on the Chess Programming Wiki.
