<div align="center">

<img
  width="250"
  alt="Tarnished Logo"
  src="assets/tarnished-logo-rounded.png">
  
<h1>Tarnished</h1>

<p>UCI Chess Engine written in C++ featuring NNUE evaluation trained from scratch. </p>

| Version | Release Date | [CCRL 40/15](https://www.computerchess.org.uk/ccrl/4040/cgi/compare_engines.cgi?family=Tarnished&print=Rating+list&print=Results+table&print=LOS+table&print=Ponder+hit+table&print=Eval+difference+table&print=Comopp+gamenum+table&print=Overlap+table&print=Score+with+common+opponents) |
| --- | --- | --- |
| 1.0 | 2025-05-10 | 2432 |
| 2.0 | 2025-05-17 | 3156 |
| 2.1 | 2025-05-24 | 3375 |
| 3.0 | 2025-06-29 | 3450* |

</div>

## Building
You can easily build Tarnished with `make`. NNUE files are stored at [tarnished-nets](https://github.com/Bobingstern/tarnished-nets). The Makefile will automatically download the default network for whatever version you are building. Tarnished makes use of [incbin](https://github.com/graphitemaster/incbin) to embed the file into the executable itself, removing the need to carry an external network along with it. To build with `make` you may 
1. Clone the repository.
2. `make`
3. Binary is at `tarnished.exe`

## Features

- Move Generation
    - Internally uses [chess-library](https://disservin.github.io/chess-library/)
- NNUE `(768->1024)x2->1x8`
    - Trained with [bullet](https://github.com/jw1912/bullet)
    - Self generated training data
    - `(piece, square, color)` input features, 8 output buckets
    - 5000 soft nodes for self play
    - 8 random plies for opening
    - ~1.7b positions
- Search
    - Principle Variation Search
    - Quiescence Search
    - Iterative Deepening
    - Aspiration Windows
    - Shared Transposition Table
    - Move Ordering
        - Staged Move Picker
        - MVV-LVA
        - TT Move
        - Killer Move Heuristic 
        - Butterfly History Heuristic
        - 1 ply Continuation History
        - 2 ply Continuation History
        - Capture History
        - SEE Move Ordering
    - Selectivity
        - TT Cutoffs
        - Reverse Futility Pruning
        - Null Move Pruning
        - Improving Heuristic
        - Razoring
        - Late Move Reductions (Factorized formulation)
        - Late Move Pruning
        - SEE Pruning (PVS and QS)
        - Singular Extensions
            - Double Extensions
            - Negative Extensions
        - Terminal Conditions (Mate, Stalemate, 3fold...)
        - Internal Iterative Reductions
 - Misc
     - Static Evaluation Correction History (Pawn, Non-Pawn, Major, Minor Hashes)
     - TT Static Evaluation Cutoffs
     - Soft time management
     - Node time management
     - Lazy SMP
     - Thread Meritocracy
     - SPSA Parameter Tuning
     - Supports FRC (Chess960)

## Non-standard UCI Commands

- `print`
    - Prints out the board, side to move, castling rights, Zobrist Hash, etc
- `eval`
    - Prints the current position's static evaluation for the side to move
- `go softnodes <nodes>`
    - Start search with a soft node limit (only checked once per iteration of deepening)
- `wait`
    - Ignore all input until search is completed. Useful for scripts
- `bench`
    - Runs an OpenBench style benchmark on 50 positions. Alternatively run `./tarnished bench`
 - `datagen name Threads value <threads>`
     - Begins data generation with the specified number of threads with viriformat output files.
     - It should create a folder with `<threads>` number of `.vf` files. If you're on windows, you can run `copy /b *.vf output.vf` to merge them all into one file for training.
     - Hyperthreading seems to be somewhat profitable
     - Send me your data!

## Credits
- The name Tarnished is a reference to a certain video game protagonist
- Thanks to everyone in the Stockfish Discord Server
- [MattBench](https://chess.n9x.co/index/) (Thanks Matt and everyone else for sharing cores, special thanks to Micpilar for helping with datagen)
- [Swedishchef](https://github.com/JonathanHallstrom) (Helping out with many engine development related things)
- [Weiss](https://github.com/TerjeKir/Weiss)
- [Stash](https://github.com/mhouppin/stash-bot)
- [Sirius](https://github.com/mcthouacbb/Sirius) (Rand for pointing out many silly mistakes)
- [Stormphrax](https://github.com/Ciekce/Stormphrax) (Ciekce for helping out with programming details and NNUE things)
- [Prelude](https://git.nocturn9x.space/Quinniboi10/Prelude)
- [Bullet](https://github.com/jw1912/bullet)
- [weather-factory](https://github.com/jnlt3/weather-factory) Local SPSA tuner for UCI chess engines