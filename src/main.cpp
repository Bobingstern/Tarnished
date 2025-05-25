#include <chrono>
#include <random>
#include <iostream>
#include <fstream>
#include <filesystem>
#include "external/chess.hpp"
#include "search.h"
#include "searcher.h"
#include "nnue.h"
#include "eval.h"
#include "uci.h"
#include "timeman.h"
#include "datagen.h"
#include "util.h"

using namespace chess;
using namespace std::chrono;


#ifndef EVALFILE
    #define EVALFILE "network/latest.bin"
#endif

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

// Thanks Weiss
// I will eventaully C++ify the UCI code
// For now it's a weird mix of C and C++ xd
void ParseTimeControl(char *str, Color color, Search::Limit &limit) {

    // Read in relevant search constraints
    int64_t mtime = 0;
    int64_t ctime = 0;
    int64_t depth = 0;
    int64_t nodes = -1;
    int64_t softnodes = -1;
    int64_t inc = 0;

    SetLimit(str, "movetime",  &mtime); 
    SetLimit(str, "depth",     &depth);
    SetLimit(str, "nodes",     &nodes);
    SetLimit(str, "softnodes",     &softnodes);

    if (mtime == 0 && depth == 0){
        SetLimit(str, color == Color::WHITE ? "wtime" : "btime", &ctime);
        SetLimit(str, color == Color::WHITE ? "winc"  : "binc" , &inc);
    }
    if (strstr(str, "infinite")){
        ctime = 0;
        mtime = 32000;
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
    limit.start();
}
  
void UCIPosition(Board &board, char *str) {

    // Set up original position. This will either be a
    // position given as FEN, or the normal start position
    if (BeginsWith(str, "position fen")){
        std::string_view fen = str+13;
        board.setFen(fen);
    }
    else { 
        board.setFen("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
    } 

    // Check if there are moves to be made from the initial position
    if ((str = strstr(str, "moves")) == NULL)
        return;

    Accumulator acc;
    
    // Loop over the moves and make them in succession
    char *move = strtok(str, " ");
    while ((move = strtok(NULL, " "))) {

        // Parse and make move
        std::string m = move;
        Move move_ = uci::uciToMove(board, m);
        board.makeMove(move_);
    }

}

static int HashInput(char *str) {
    int hash = 0;
    int len = 1;
    while (*str && *str != ' ')
        hash ^= *(str++) ^ len++;
    return hash;
};

void UCISetOption(Searcher &searcher, char *str) {

    // Sets the size of the transposition table
    if (OptionName(str, "Hash")) {
        searcher.TT.resize((uint64_t)atoi(OptionValue(str)));
        std::cout << "Hash Table successfully resized" << std::endl;
    // Sets number of threads to use for searching
    } else if (OptionName(str, "Threads")) {
        searcher.initialize(atoi(OptionValue(str)));
    }
    else if (OptionName(str, "UCI_ShowWDL")) {
        std::string opt = OptionValue(str);
        searcher.toggleWDL(opt == "true");
    }
}
void UCIInfo(){
    std::cout << "id name Tarnished v2.1 (Ambition)\n";
    std::cout << "id author Anik Patel\n";
    std::cout << "option name Hash type spin default 16 min 2 max 65536\n";
    std::cout << "option name Threads type spin default 1 min 1 max 256\n";
    std::cout << "option name UCI_ShowWDL type check default false\n";
    std::cout << "uciok" << std::endl; 
}

void UCIEvaluate(Board &board){
    Accumulator a;
    a.refresh(board);
    std::cout << network.inference(&board, a) << std::endl;
}

void UCIGo(Searcher &searcher, Board &board, char *str){
    searcher.stop();

    Search::Limit limit = Search::Limit();
    ParseTimeControl(str, board.sideToMove(), limit);

    searcher.start(board, limit);
    //searcher.stop();
}   

void BeginDatagen(char *str){
    // Same way as setoption
    // datagen name Threads value 16
    int threadc = DATAGEN_THREADS; // 8 default
    if (OptionName(str, "Threads")){
        threadc = atoi(OptionValue(str));
    }
    std::cout << "Launching Data Generation with " << threadc << " threads" << std::endl;
    startDatagen(threadc); 
}

int main(int agrc, char *argv[]){
    //r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1
    initLookups();
	Board board = Board();
    //network.randomize();
    #if defined(_MSC_VER) && !defined(__clang__)
        network.loadNetwork(EVALFILE);
        std::cerr << "WARNING: This file was compiled with MSVC, this means that an nnue was NOT embedded into the exe." << std::endl;
    #else
            network = *reinterpret_cast<const NNUE*>(gEVALData);
    #endif
    

    Search::fillLmr();
    Searcher searcher = Searcher();
    searcher.toggleWDL(false); // Default display wdl
    searcher.initialize(1); // Default one thread
    searcher.reset();

    if (agrc > 1){
        std::string arg = argv[1];
        if (arg == "bench")
            Search::bench();
        return 0;
    }
    char str[INPUT_SIZE];
    while (GetInput(str)) {
        switch (HashInput(str)) {
            case GO         : UCIGo(searcher, board, str);                break;
            case UCI        : UCIInfo();                                  break;
            case ISREADY    : std::cout << "readyok" << std::endl;        break;
            case POSITION   : UCIPosition(board, str);                    break;
            case SETOPTION  : UCISetOption(searcher, str);                break;
            case UCINEWGAME : searcher.reset();                           break;
            case STOP       : searcher.stop();                            break;
            case QUIT       : searcher.stop();                            return 0;

            // Non Standard
            case PRINT      : std::cout << board << std::endl;            break;
            case EVAL       : UCIEvaluate(board);                         break;
            case BENCH      : Search::bench();                            break;
            case DATAGEN    : BeginDatagen(str);                          break;

        }
    }

	return 0;
}