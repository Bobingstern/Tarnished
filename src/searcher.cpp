#include "searcher.h"
#include "external/chess.hpp"
#include "tt.h"
#include "util.h"
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <vector>

Search::ThreadInfo::ThreadInfo(ThreadType t, Searcher& s)
    : type(t), searcher(s) {
    board = Board();
    stopped = false;
    exiting = false;
    thread = std::thread(&Search::ThreadInfo::idle, this);
    reset();
};

Search::ThreadInfo::ThreadInfo(int id, Searcher& s)
    : threadId(id), searcher(s) {
    type = id == 0 ? ThreadType::MAIN : ThreadType::SECONDARY;
    board = Board();
    stopped = false;
    exiting = false;
    thread = std::thread(&Search::ThreadInfo::idle, this);
    reset();
};

void Search::ThreadInfo::exit() {
    exiting = true;
}

Search::ThreadInfo::~ThreadInfo() {
    thread.join();
}

void Search::ThreadInfo::startSearching() {
    nodes = 0;
    bestMove = Move::NO_MOVE;
    bestRootScore = -EVAL_INF;
    board = searcher.board;
    searchStack[STACK_OVERHEAD].accumulator->refresh(board);
    bucketCache = InputBucketCache();
    
    Search::iterativeDeepening(*this, searcher.limit);

    if (type == ThreadType::MAIN) {
        searcher.stopSearching();
        // Possible thread voting for future
        ThreadInfo* bestSearcher = this;
        searcher.bestScore = bestSearcher->bestRootScore;
        
        if (searcher.printInfo)
            std::cout << "\nbestmove "
                      << uci::moveToUci(bestSearcher->bestMove,
                                        searcher.board.chess960())
                      << std::endl;
    }
}

void Search::ThreadInfo::waitForSearchFinished() {
    
}

void Search::ThreadInfo::idle() {
    while (true) {
        searcher.idleBarrier->arrive_and_wait();
        if (exiting)
            return;
        {
            std::shared_lock lockGuard{searcher.mutex};
            (void)searcher.startedBarrier->arrive();
            startSearching();
        }
    }
}