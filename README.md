


<p align="center">
</p>
<h1 align="center">Tarnished</h1>

UCI Chess Engine written in C++ featuring NNUE evaluation trained from scratch. The name is a reference to a certain video game protagonist

As of right now, all tests and data generation have been conducted on my personal PCs with the use of [cutechess-cli](https://github.com/cutechess/cutechess/tree/master) but will eventually migrate to [OpenBench](https://github.com/AndyGrant/OpenBench)

## Estimated Strength
So far, Tarnished can defeat [stash-30](https://github.com/mhouppin/stash-bot) at `40+0.4` which is estimated ~3160 on CCRL. This is not definitive and will be updated when Tarnished gets an official CCRL estimate. Tests were run on an `Intel(R) Core(TM) i7-10700F`. Here are the results after around 750 games
```
Engine     | tarnished vs stash-30.0
TC         | 40+0.04
Elo        | 37.9 +/- 19.8
Games      | N: 746 W: 274 L: 193 D: 279 [0.554]
```

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
    - Selectivity
        - Reverse Futility Pruning
        - Null Move Pruning
        - Improving Heuristic
        - Late Move Reductions
        - Late Move Pruning
        - QS SEE Pruning
        - Terminal Conditions (Mate, Stalemate, 3fold...)
        - Internal Iterative Reductions
 - Misc
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
- [Weiss](https://github.com/TerjeKir/Weiss)
- [Stash](https://github.com/mhouppin/stash-bot)
- [Sirius](https://github.com/mcthouacbb/Sirius) 
- [Stormphrax](https://github.com/Ciekce/Stormphrax) (Ciekce is the goat)
- [Prelude](https://git.nocturn9x.space/Quinniboi10/Prelude) (Especially for base NNUE code)
- [Bullet](https://github.com/jw1912/bullet)