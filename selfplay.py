import subprocess, sys, time

FENS = [
 "startpos",
 "fen rnbqkbnr/pppp1ppp/8/4p3/4P3/8/PPPP1PPP/RNBQKBNR w KQkq - 0 2",
 "fen rnbqkbnr/pp1ppppp/8/2p5/4P3/8/PPPP1PPP/RNBQKBNR w KQkq - 0 2",
 "fen rnbqkb1r/pppppppp/5n2/8/3P4/8/PPP1PPPP/RNBQKBNR w KQkq - 1 2",
 "fen r1bqkbnr/pppp1ppp/2n5/4p3/2B1P3/5N2/PPPP1PPP/RNBQK2R b KQkq - 3 3",
 "fen rnbqkbnr/ppp1pppp/8/3p4/2PP4/8/PP2PPPP/RNBQKBNR b KQkq - 0 2",
]

p = subprocess.Popen(["./engine"], stdin=subprocess.PIPE, stdout=subprocess.PIPE, text=True, bufsize=1)
def send(s): p.stdin.write(s + "\n"); p.stdin.flush()
def read_until(prefix):
    lines = []
    while True:
        l = p.stdout.readline()
        if not l: raise RuntimeError("engine died")
        lines.append(l.strip())
        if l.startswith(prefix): return lines

send("uci"); read_until("uciok")
results = []
for g, base in enumerate(FENS):
    send("ucinewgame"); send("isready"); read_until("readyok")
    moves = []
    outcome = "max plies"
    for ply in range(300):
        pos = f"position {base}" + (" moves " + " ".join(moves) if moves else "")
        send(pos)
        send("go movetime 40")
        lines = read_until("bestmove")
        bad = [l for l in lines if "illegal" in l or "bad FEN" in l]
        if bad: print("PROBLEM:", bad, "after", moves[-3:]); sys.exit(1)
        bm = lines[-1].split()[1]
        if bm == "0000":
            last = [l for l in lines if l.startswith("info depth")]
            outcome = "game over (no legal moves) after %d plies" % ply
            break
        moves.append(bm)
    print(f"game {g+1}: {outcome}; last moves: {' '.join(moves[-4:])}")
send("quit")
print("done, no illegal moves, no crashes")
