



<p align="center">
</p>
<h1 align="center">Tarnished</h1>

UCI Chess Engine written in C++ featuring NNUE evaluation trained from scratch. The name is a reference to a certain video game protagonist

## Estimated Strength

| Version | Release Date | [CCRL 40/15](https://www.computerchess.org.uk/ccrl/4040/cgi/compare_engines.cgi?class=None&only_best_in_class=on&num_best_in_class=1&e=Tarnished+1.0-Renown+64-bit&e=Tarnished+2.0-Ambition+64-bit&print=Rating+list&profile_step=50&profile_numbers=1&print=Results+table&print=LOS+table&table_size=100&ct_from_elo=0&ct_to_elo=10000&match_length=30&cross_tables_for_best_versions_only=1&sort_tables=by+rating&diag=0&reference_list=None&recalibrate=no) |
| --- | --- | --- | --- |
| 1.0 | 2025-05-10 | 2432 |
| 2.0 | 2025-05-17 | 3152 |
| 2.1 | 2025-05-24 | ~3300* |

## Building
It seems like the `Makefile` is slightly faster than using `CMake` but you may use whichever one you wish. Make sure to have an NNUE file under `network/latest.bin` if you plan on building the project. Tarnished makes use of [incbin](https://github.com/graphitemaster/incbin) to embed the file into the executable itself, removing the need to carry an external network along with it. To build with `make` you may 
1. Clone the repository.
2. `make`
3. Binary is at `tarnished.exe`

Alternatively, with `CMake`

1. Clone the repository.
2. `mkdir build && cd build`
3. `cmake .. -G Ninja -DCMAKE_BUILD_TYPE=Release`
4. `cmake --build .`
5. Binary is at `build/Tarnished.exe`

## Features

- Move Generation
    - Internally uses [chess-library](https://disservin.github.io/chess-library/)
- NNUE `(768->512)x2->1x8`
    - Trained with [bullet](https://github.com/jw1912/bullet)
    - Self generated training data
    - `(piece, square, color)` input features, 8 output buckets
    - 5000 soft nodes for self play
    - 8 random plies for opening
- Search
    - Principle Variation Search
    - Quiescence Search
    - Iterative Deepening
    - Aspiration Windows
    - Shared Transposition Table
    - Move Ordering
        - MVV-LVA
        - TT Move
        - Killer Move Heuristic 
        - Butterfly History Heuristic
        - 1 ply Continuation History
        - Capture History
        - SEE Move Ordering
    - Selectivity
        - Reverse Futility Pruning
        - Null Move Pruning
        - Improving Heuristic
        - Late Move Reductions
        - Late Move Pruning
        - SEE Pruning (qsearch)
        - Singular Extensions
            - Double Extensions
            - Negative Extensions
        - Terminal Conditions (Mate, Stalemate, 3fold...)
        - Internal Iterative Reductions
 - Misc
     - Static Evaluation Correction History (Pawn hash indexed)
     - Soft time management
     - Lazy SMP (functional but not tested thoroughly)

## Non-standard UCI Commands

- `print`
    - Prints out the board, side to move, castling rights, Zobrist Hash, etc
- `eval`
    - Prints the current position's static evaluation for the side to move
- `go softnodes <nodes>`
    - Start search with a soft node limit (only checked once per iteration of deepening)
- `bench`
    - Runs an OpenBench style benchmark on 50 positions. Alternatively run `./tarnished bench`
 - `datagen name Threads value <threads>`
     - Begins data generation with the specified number of threads with viriformat output files.
     - It should create a folder with `<threads>` number of `.vf` files. If you're on windows, you can run `copy /b *.vf output.vf` to merge them all into one file for training.
     - Hyperthreading seems to be somewhat profitable
     - Send me your data!

## Credits
- Stockfish Discord Server
- [MattBench](https://chess.n9x.co/index/) (Thanks Matt for letting me join)
- [Weiss](https://github.com/TerjeKir/Weiss)
- [Stash](https://github.com/mhouppin/stash-bot)
- [Sirius](https://github.com/mcthouacbb/Sirius) (Rand for pointing out many silly mistakes)
- [Stormphrax](https://github.com/Ciekce/Stormphrax) (Ciekce for helping out with programming details and NNUE things)
- [Prelude](https://git.nocturn9x.space/Quinniboi10/Prelude)
- [Bullet](https://github.com/jw1912/bullet)