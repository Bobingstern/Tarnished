#include "datagen.h"
#include "eval.h"
#include "external/chess.hpp"
#include "nnue.h"
#include "parameters.h"
#include "search.h"
#include "searcher.h"
#include "timeman.h"
#include "uci.h"
#include "util.h"
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <random>

using namespace chess;
using namespace std::chrono;

#ifdef _MSC_VER
    #define MSVC
    #pragma push_macro("_MSC_VER")
    #undef _MSC_VER
#endif

#include "external/incbin.h"

#ifdef MSVC
    #pragma pop_macro("_MSC_VER")
    #undef MSVC
#endif

#if !defined(_MSC_VER) || defined(__clang__)
INCBIN(EVAL, EVALFILE);
#endif

NNUE network;
bool PRETTY_PRINT = true;
// Thanks Weiss
// I will eventaully C++ify the UCI code
// For now it's a weird mix of C and C++ xd
void ParseTimeControl(char* str, Color color, Search::Limit& limit, bool onlySoft) {

    // Read in relevant search constraints
    int64_t mtime = 0;
    int64_t ctime = 0;
    int64_t depth = 0;
    int64_t nodes = -1;
    int64_t softnodes = -1;
    int64_t inc = 0;

    SetLimit(str, "movetime", &mtime);
    SetLimit(str, "depth", &depth);
    SetLimit(str, "nodes", &nodes);
    SetLimit(str, "softnodes", &softnodes);

    if (mtime == 0 && depth == 0) {
        SetLimit(str, color == Color::WHITE ? "wtime" : "btime", &ctime);
        SetLimit(str, color == Color::WHITE ? "winc" : "binc", &inc);
    }
    if (strstr(str, "infinite")) {
        ctime = 0;
        mtime = 0;
        nodes = -1;
        softnodes = -1;
        depth = 0;
    }
    limit.ctime = ctime;
    limit.movetime = mtime;
    limit.inc = inc;
    limit.depth = depth;

    limit.maxnodes = nodes;
    limit.softnodes = softnodes;

    if (onlySoft) {
        limit.maxnodes = -1;
        limit.softnodes = std::max(nodes, softnodes);
    }
    limit.start();
}

void UCIPosition(Board& board, char* str) {

    // Set up original position. This will either be a
    // position given as FEN, or the normal start position
    if (BeginsWith(str, "position fen")) {
        std::string_view fen = str + 13;
        board.setFen(fen);
    } else {
        board.setFen("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
    }

    // Check if there are moves to be made from the initial position
    if ((str = strstr(str, "moves")) == NULL)
        return;

    char* move = strtok(str, " ");
    while ((move = strtok(NULL, " "))) {

        // Parse and make move
        std::string m = move;
        Move move_ = uci::uciToMove(board, m);
        board.makeMove(move_);
    }
}

static int HashInput(char* str) {
    int hash = 0;
    int len = 1;
    while (*str && *str != ' ')
        hash ^= *(str++) ^ len++;
    return hash;
};

void UCISetOption(Searcher& searcher, Board& board, char* str) {

    // Sets the size of the transposition table
    if (OptionName(str, "Hash")) {
        searcher.TT.resize((uint64_t)atoi(OptionValue(str)));
        std::cout << "Hash Table successfully resized" << std::endl;
        // Sets number of threads to use for searching
    } else if (OptionName(str, "Threads")) {
        searcher.initialize(atoi(OptionValue(str)));
    } else if (OptionName(str, "UCI_ShowWDL")) {
        std::string opt = OptionValue(str);
        searcher.toggleWDL(opt == "true");
    } else if (OptionName(str, "UCI_Chess960")) {
        std::string opt = OptionValue(str);
        board.set960(opt == "true");
    } else if (OptionName(str, "UseSoftNodes")) {
        std::string opt = OptionValue(str);
        searcher.toggleSoft(opt == "true");
    } else if (OptionName(str, "NormalizeEval")) {
        std::string opt = OptionValue(str);
        searcher.toggleNorm(opt == "true");
    } else {
        for (auto& param : tunables()) {
            const char* p = param.name.c_str();
            if (OptionName(str, p)) {
                param.value = atoi(OptionValue(str));
            }
        }
        for (int i = 0; i < LMR_THREE_PAIR.size(); i++) {
            std::string a = "LMR_ONE_PAIR_" + std::to_string(i);
            std::string b = "LMR_TWO_PAIR_" + std::to_string(i);
            std::string c = "LMR_THREE_PAIR_" + std::to_string(i);
            const char* ac = a.c_str();
            const char* bc = b.c_str();
            const char* cc = c.c_str();
            if (OptionName(str, ac)) {
                LMR_ONE_PAIR[i] = atoi(OptionValue(str));
            }
            if (OptionName(str, bc)) {
                LMR_TWO_PAIR[i] = atoi(OptionValue(str));
            }
            if (OptionName(str, cc)) {
                LMR_THREE_PAIR[i] = atoi(OptionValue(str));
            }
        }

        Search::fillLmr();
        PieceValue[0] = PAWN_VALUE();
        PieceValue[1] = KNIGHT_VALUE();
        PieceValue[2] = BISHOP_VALUE();
        PieceValue[3] = ROOK_VALUE();
        PieceValue[4] = QUEEN_VALUE();
    }
}
void UCIInfo() {
    PRETTY_PRINT = false;
    std::cout << "id name Tarnished v4.0 (Consort)\n";
    std::cout << "id author Anik Patel\n";
    std::cout << "option name Hash type spin default 16 min 2 max 16777216\n";
    std::cout << "option name Threads type spin default 1 min 1 max 1024\n";
    std::cout << "option name UCI_ShowWDL type check default true\n";
    std::cout << "option name UCI_Chess960 type check default false\n";
    std::cout << "option name UseSoftNodes type check default false\n";
    std::cout << "option name NormalizeEval type check default true\n";
#ifdef TUNE
    for (auto& param : tunables()) {
        std::cout << "option name " << param.name << " type spin default " << param.defaultValue << " min " << param.min
                  << " max " << param.max << std::endl;
    }
#endif

#if defined(LMR_TUNE) || defined(TUNE)
    #ifndef TUNE
    for (auto& param : tunables()) {
        if (param.name.substr(0, 3) != "LMR")
            continue;
        std::cout << "option name " << param.name << " type spin default " << param.defaultValue << " min " << param.min
                  << " max " << param.max << std::endl;
    }
    #endif
    // For pair
    for (int i = 0; i < LMR_ONE_PAIR.size(); i++) {
        std::string s = "LMR_ONE_PAIR_" + std::to_string(i);
        std::cout << "option name " << s << " type spin default " << LMR_ONE_PAIR[i] << " min -2048 max 2048"
                  << std::endl;
    }
    for (int i = 0; i < LMR_TWO_PAIR.size(); i++) {
        std::string s = "LMR_TWO_PAIR_" + std::to_string(i);
        std::cout << "option name " << s << " type spin default " << LMR_TWO_PAIR[i] << " min -2048 max 2048"
                  << std::endl;
    }
    for (int i = 0; i < LMR_THREE_PAIR.size(); i++) {
        std::string s = "LMR_THREE_PAIR_" + std::to_string(i);
        std::cout << "option name " << s << " type spin default " << LMR_THREE_PAIR[i] << " min -2048 max 2048"
                  << std::endl;
    }
#endif
    std::cout << "uciok" << std::endl;
}

void UCIEvaluate(Board& board) {
    Accumulator a;
    a.refresh(board);
    int eval = network.inference(board, a);
    std::cout << "Raw: " << eval << std::endl;
    std::cout << "Normalized: " << scaleEval(eval, board) << std::endl;
}

void UCIGo(Searcher& searcher, Board& board, char* str) {
    // searcher.stop();
    Search::Limit limit = Search::Limit();
    ParseTimeControl(str, board.sideToMove(), limit, searcher.useSoft);

    searcher.startSearching(board, limit);
    // searcher.stop();
}

void BeginDatagen(char* str, bool isDFRC) {
    // Same way as setoption
    // datagen name Threads value 16
    int threadc = DATAGEN_THREADS; // 8 default
    if (OptionName(str, "Threads")) {
        threadc = atoi(OptionValue(str));
    }
    std::cout << "Launching Data Generation with " << threadc << " threads" << std::endl;
    if (isDFRC)
        std::cout << "DFRC Enabled" << std::endl;
    startDatagen(threadc, isDFRC);
}
// Benchmark for OpenBench
void bench(Searcher& searcher) {
    int64_t totalNodes = 0;
    int64_t totalMS = 0;

    std::cout << "Benchmark started at depth " << (int)BENCH_DEPTH << std::endl;
    // Thanks Prelude
    std::array<std::string, 50> fens = {"r3k2r/2pb1ppp/2pp1q2/p7/1nP1B3/1P2P3/P2N1PPP/R2QK2R w KQkq a6 0 14",
                                        "4rrk1/2p1b1p1/p1p3q1/4p3/2P2n1p/1P1NR2P/PB3PP1/3R1QK1 b - - 2 24",
                                        "r3qbrk/6p1/2b2pPp/p3pP1Q/PpPpP2P/3P1B2/2PB3K/R5R1 w - - 16 42",
                                        "6k1/1R3p2/6p1/2Bp3p/3P2q1/P7/1P2rQ1K/5R2 b - - 4 44",
                                        "8/8/1p2k1p1/3p3p/1p1P1P1P/1P2PK2/8/8 w - - 3 54",
                                        "7r/2p3k1/1p1p1qp1/1P1Bp3/p1P2r1P/P7/4R3/Q4RK1 w - - 0 36",
                                        "r1bq1rk1/pp2b1pp/n1pp1n2/3P1p2/2P1p3/2N1P2N/PP2BPPP/R1BQ1RK1 b - - 2 10",
                                        "3r3k/2r4p/1p1b3q/p4P2/P2Pp3/1B2P3/3BQ1RP/6K1 w - - 3 87",
                                        "2r4r/1p4k1/1Pnp4/3Qb1pq/8/4BpPp/5P2/2RR1BK1 w - - 0 42",
                                        "4q1bk/6b1/7p/p1p4p/PNPpP2P/KN4P1/3Q4/4R3 b - - 0 37",
                                        "2q3r1/1r2pk2/pp3pp1/2pP3p/P1Pb1BbP/1P4Q1/R3NPP1/4R1K1 w - - 2 34",
                                        "1r2r2k/1b4q1/pp5p/2pPp1p1/P3Pn2/1P1B1Q1P/2R3P1/4BR1K b - - 1 37",
                                        "r3kbbr/pp1n1p1P/3ppnp1/q5N1/1P1pP3/P1N1B3/2P1QP2/R3KB1R b KQkq b3 0 17",
                                        "8/6pk/2b1Rp2/3r4/1R1B2PP/P5K1/8/2r5 b - - 16 42",
                                        "1r4k1/4ppb1/2n1b1qp/pB4p1/1n1BP1P1/7P/2PNQPK1/3RN3 w - - 8 29",
                                        "8/p2B4/PkP5/4p1pK/4Pb1p/5P2/8/8 w - - 29 68",
                                        "3r4/ppq1ppkp/4bnp1/2pN4/2P1P3/1P4P1/PQ3PBP/R4K2 b - - 2 20",
                                        "5rr1/4n2k/4q2P/P1P2n2/3B1p2/4pP2/2N1P3/1RR1K2Q w - - 1 49",
                                        "1r5k/2pq2p1/3p3p/p1pP4/4QP2/PP1R3P/6PK/8 w - - 1 51",
                                        "q5k1/5ppp/1r3bn1/1B6/P1N2P2/BQ2P1P1/5K1P/8 b - - 2 34",
                                        "r1b2k1r/5n2/p4q2/1ppn1Pp1/3pp1p1/NP2P3/P1PPBK2/1RQN2R1 w - - 0 22",
                                        "r1bqk2r/pppp1ppp/5n2/4b3/4P3/P1N5/1PP2PPP/R1BQKB1R w KQkq - 0 5",
                                        "r1bqr1k1/pp1p1ppp/2p5/8/3N1Q2/P2BB3/1PP2PPP/R3K2n b Q - 1 12",
                                        "r1bq2k1/p4r1p/1pp2pp1/3p4/1P1B3Q/P2B1N2/2P3PP/4R1K1 b - - 2 19",
                                        "r4qk1/6r1/1p4p1/2ppBbN1/1p5Q/P7/2P3PP/5RK1 w - - 2 25",
                                        "r7/6k1/1p6/2pp1p2/7Q/8/p1P2K1P/8 w - - 0 32",
                                        "r3k2r/ppp1pp1p/2nqb1pn/3p4/4P3/2PP4/PP1NBPPP/R2QK1NR w KQkq - 1 5",
                                        "3r1rk1/1pp1pn1p/p1n1q1p1/3p4/Q3P3/2P5/PP1NBPPP/4RRK1 w - - 0 12",
                                        "5rk1/1pp1pn1p/p3Brp1/8/1n6/5N2/PP3PPP/2R2RK1 w - - 2 20",
                                        "8/1p2pk1p/p1p1r1p1/3n4/8/5R2/PP3PPP/4R1K1 b - - 3 27",
                                        "8/4pk2/1p1r2p1/p1p4p/Pn5P/3R4/1P3PP1/4RK2 w - - 1 33",
                                        "8/5k2/1pnrp1p1/p1p4p/P6P/4R1PK/1P3P2/4R3 b - - 1 38",
                                        "8/8/1p1kp1p1/p1pr1n1p/P6P/1R4P1/1P3PK1/1R6 b - - 15 45",
                                        "8/8/1p1k2p1/p1prp2p/P2n3P/6P1/1P1R1PK1/4R3 b - - 5 49",
                                        "8/8/1p4p1/p1p2k1p/P2n1P1P/4K1P1/1P6/3R4 w - - 6 54",
                                        "8/8/1p4p1/p1p2k1p/P2n1P1P/4K1P1/1P6/6R1 b - - 6 59",
                                        "8/5k2/1p4p1/p1pK3p/P2n1P1P/6P1/1P6/4R3 b - - 14 63",
                                        "8/1R6/1p1K1kp1/p6p/P1p2P1P/6P1/1Pn5/8 w - - 0 67",
                                        "1rb1rn1k/p3q1bp/2p3p1/2p1p3/2P1P2N/PP1RQNP1/1B3P2/4R1K1 b - - 4 23",
                                        "4rrk1/pp1n1pp1/q5p1/P1pP4/2n3P1/7P/1P3PB1/R1BQ1RK1 w - - 3 22",
                                        "r2qr1k1/pb1nbppp/1pn1p3/2ppP3/3P4/2PB1NN1/PP3PPP/R1BQR1K1 w - - 4 12",
                                        "2r2k2/8/4P1R1/1p6/8/P4K1N/7b/2B5 b - - 0 55",
                                        "6k1/5pp1/8/2bKP2P/2P5/p4PNb/B7/8 b - - 1 44",
                                        "2rqr1k1/1p3p1p/p2p2p1/P1nPb3/2B1P3/5P2/1PQ2NPP/R1R4K w - - 3 25",
                                        "r1b2rk1/p1q1ppbp/6p1/2Q5/8/4BP2/PPP3PP/2KR1B1R b - - 2 14",
                                        "6r1/5k2/p1b1r2p/1pB1p1p1/1Pp3PP/2P1R1K1/2P2P2/3R4 w - - 1 36",
                                        "rnbqkb1r/pppppppp/5n2/8/2PP4/8/PP2PPPP/RNBQKBNR b KQkq c3 0 2",
                                        "2rr2k1/1p4bp/p1q1p1p1/4Pp1n/2PB4/1PN3P1/P3Q2P/2RR2K1 w - f6 0 20",
                                        "3br1k1/p1pn3p/1p3n2/5pNq/2P1p3/1PN3PP/P2Q1PB1/4R1K1 w - - 0 23",
                                        "2r2b2/5p2/5k2/p1r1pP2/P2pB3/1P3P2/K1P3R1/7R w - - 23 93"};

    TimeLimit timer = TimeLimit();
    searcher.printInfo = false;
    searcher.waitForSearchFinished();
    searcher.reset();
    for (auto fen : fens) {
        timer.start();
        Board board(fen);
        Search::Limit limit = Search::Limit();
        limit.depth = (int64_t)BENCH_DEPTH;
        limit.movetime = 0;
        limit.ctime = 0;
        limit.start();

        searcher.startSearching(board, limit);
        searcher.waitForSearchFinished();
        int ms = timer.elapsed();
        totalMS += ms;
        totalNodes += searcher.nodeCount();

        std::cout << "-----------------------------------------------------------------------" << std::endl;
        std::cout << "FEN: " << fen << std::endl;
        std::cout << "Nodes: " << searcher.nodeCount() << std::endl;
        std::cout << "Time: " << ms << "ms" << std::endl;
        std::cout << "-----------------------------------------------------------------------\n" << std::endl;
    }
    std::cout << "Completed Benchmark" << std::endl;
    std::cout << "Total Nodes: " << totalNodes << std::endl;
    std::cout << "Elapsed Time: " << totalMS << "ms" << std::endl;
    int nps = static_cast<int64_t>((totalNodes / totalMS) * 1000);
    std::cout << "Average NPS: " << nps << std::endl;
    std::cout << totalNodes << " nodes " << nps << " nps" << std::endl;

    searcher.printInfo = true;
}

int main(int agrc, char* argv[]) {
    // r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1
    initLookups();
    Board board = Board();
// network.randomize();
#if defined(_MSC_VER) && !defined(__clang__)
    network.load(EVALFILE);
    std::cerr << "WARNING: This file was compiled with MSVC, this means that an nnue was NOT embedded into the exe."
              << std::endl;
#else
    network = *reinterpret_cast<const NNUE*>(gEVALData);
#endif

    Console console; // Windows nonsense
#if defined(_WIN32)
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
    if (!console.vtEnabled) {
        // Old Windows fallback
        console.setColor(FOREGROUND_RED | FOREGROUND_INTENSITY);
        std::cout << "Pretty Print (WinAPI fallback)\n";
        console.setColor(FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
    }
#endif

    Search::fillLmr();
    Searcher searcher = Searcher();
    searcher.toggleWDL(true); // Default display wdl
    searcher.initialize(1);   // Default one thread
    searcher.reset();

    if (agrc > 1) {
        std::string arg = argv[1];
        if (arg == "bench"){
            bench(searcher);
            searcher.exit();
            return 0;
        }
        if (arg.substr(0, 7) == "genfens"){
            handleGenfens(searcher, arg);
            if (agrc > 2) {
                arg = argv[2];
                if (arg == "quit"){
                    searcher.exit();
                    return 0;
                }
            }
            searcher.reset();
        }
    }

    // Print Ascii
    tarnishedAscii();

    char str[INPUT_SIZE];
    while (GetInput(str)) {
        switch (HashInput(str)) {
            case GO         : UCIGo(searcher, board, str);                break;
            case UCI        : UCIInfo();                                  break;
            case ISREADY    : std::cout << "readyok" << std::endl;        break;
            case POSITION   : UCIPosition(board, str);                    break;
            case SETOPTION  : UCISetOption(searcher, board, str);         break;
            case UCINEWGAME : searcher.reset();                           break;
            case STOP       : searcher.stopSearching();                   break;
            case QUIT       : searcher.stopSearching(); searcher.exit();  return 0;

            // Non Standard
            case PRINT      : printBoard(board);                          break;
            case EVAL       : UCIEvaluate(board);                         break;
            case BENCH      : bench(searcher);                            break;
            case DATAGEN    : BeginDatagen(str, board.chess960());        break;
            case WAIT       : searcher.waitForSearchFinished();           break;
            case CONFIG     : printOBConfig();                            break;

        }
    }

    return 0;
}