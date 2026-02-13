

<div align="center">

<img
  width="250"
  alt="Tarnished Logo"
  src="assets/tarnished-logo-rounded.png">
  
<h1>Tarnished</h1>

<p>Strong UCI Chess Engine written in C++ featuring NNUE evaluation trained from scratch. </p>

As of `2026-02-13`, Tarnished places at **#14** on [SP-CC](https://www.sp-cc.de/index.htm). It can be found playing at [TCEC](https://tcec-chess.com/#) and [CCC](https://www.chess.com/computer-chess-championship)

| Version | Release Date | [CCRL 40/15](https://www.computerchess.org.uk/ccrl/4040/cgi/compare_engines.cgi?family=Tarnished&print=Rating+list&print=Results+table&print=LOS+table&print=Ponder+hit+table&print=Eval+difference+table&print=Comopp+gamenum+table&print=Overlap+table&print=Score+with+common+opponents) | [CCRL Blitz](https://www.computerchess.org.uk/ccrl/404/cgi/compare_engines.cgi?class=Single-CPU+engines&only_best_in_class=on&num_best_in_class=1&print=Rating+list) |
| --- | --- | --- | --- |
| 1.0 | 2025-05-10 | 2432 | 2485
| 2.0 | 2025-05-17 | 3156 | -
| 2.1 | 2025-05-24 | 3375 | -
| 3.0 | 2025-06-29 | 3495 | 3605
| 4.0 | 2025-08-22 | 3571 | 3663
| 5.0 | 2026-02-07 | -    | 3700*

</div>

## Building
You can easily build Tarnished with `make`. NNUE files are stored at [tarnished-nets](https://github.com/Bobingstern/tarnished-nets). The Makefile will automatically download the default network for whatever version you are building. Tarnished makes use of [incbin](https://github.com/graphitemaster/incbin) to embed the file into the executable itself, removing the need to carry an external network along with it. To build with `make` you may 
1. Clone the repository.
2. `make`
3. Binary is at `tarnished.exe`

## Features

- Move Generation
    - Internally uses [chess-library](https://disservin.github.io/chess-library/)
- NNUE `(768x16hm->1536)x2->1x8`
    - Trained with [bullet](https://github.com/jw1912/bullet)
    - Self generated training data
    - `(piece, square, color)` input features, 16 king buckets, 8 output buckets
    - Horizontal Mirroring
    - Finny Tables
    - Lazy Updates
    - Fused Refresh
    - 5000/20000 soft nodes for self play
    - 8 random plies for opening
    - Relabeled with `(768x8hm->4096)x2->(96->192->192->1)x8` network
    - ~22b positions
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
        - 1/2/4 ply Continuation History
        - Pawn History
        - Capture History
        - SEE Move Ordering
        - Threats Move Ordering
    - Selectivity
        - TT Cutoffs
        - Reverse Futility Pruning
        - Null Move Pruning
        - Improving Heuristic
        - Razoring
        - Late Move Reductions (Factorized formulation)
        - Late Move Pruning
        - SEE Pruning (PVS and QS)
        - Forward Futility Pruning
        - Bad Noisy Futility Pruning
        - Singular Extensions
            - Double and Triple Extensions
            - Negative Extensions
            - MultiCut
        - Internal Iterative Reductions
  - Time Management
      - Soft Time Limit
      - Node Fraction
      - Complexity Estimate
      - Best Move Stability
 - Misc
     - Static Evaluation Correction History (Pawn, Non-Pawn, Major, Minor Hashes, Continuation)
     - TT Static Evaluation
     - TT Clusters
     - TT Aging
     - PVS Fail Firm
     - Lazy SMP
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

## Credits
- The name Tarnished is a reference to a certain video game protagonist
- Thanks to everyone in the Stockfish Discord Server
- [MattBench](https://chess.n9x.co/index/) (Thanks Matt and everyone else for sharing cores, special thanks to Micpilar for helping with datagen)
- [Rensselaer Polytechnic Institute (CCI)](https://cci.rpi.edu/) (Thanks to my university for letting me use their enourmous computing clusters for data generation and training nets)
- [Swedishchef](https://github.com/JonathanHallstrom) (Helping out with many engine development related things)
- [Dan](https://github.com/kelseyde) (Tossing around ideas and helping out with NNUE and other engine related things)
- [Weiss](https://github.com/TerjeKir/Weiss)
- [Stash](https://github.com/mhouppin/stash-bot)
- [Sirius](https://github.com/mcthouacbb/Sirius) (Rand for pointing out many silly mistakes)
- [Stormphrax](https://github.com/Ciekce/Stormphrax) (Ciekce for helping out with programming details and NNUE things)
- [Prelude](https://git.nocturn9x.space/Quinniboi10/Prelude)
- [Bullet](https://github.com/jw1912/bullet)
- [weather-factory](https://github.com/jnlt3/weather-factory) Local SPSA tuner for UCI chess engines
