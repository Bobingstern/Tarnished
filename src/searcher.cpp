#include "searcher.h"
#include "external/chess.hpp"
#include "tt.h"
#include "util.h"
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <vector>

Search::ThreadInfo::ThreadInfo(ThreadType t, TTable& tt, Searcher* s)
    : type(t), TT(tt), searcher(s) {
    board = Board();
    thread = std::thread(&Search::ThreadInfo::idle, this);
    reset();
};

Search::ThreadInfo::ThreadInfo(int id, TTable& tt, Searcher* s)
    : threadId(id), TT(tt), searcher(s) {
    type = id == 0 ? ThreadType::MAIN : ThreadType::SECONDARY;
    board = Board();
    thread = std::thread(&Search::ThreadInfo::idle, this);
    reset();
};

void Search::ThreadInfo::exit() {
    {
        std::lock_guard<std::mutex> lock(mutex);
        stops.searching = true;
        stops.exiting = true;
    }
    cv.notify_all();
    if (thread.joinable())
        thread.join();
}

void Search::ThreadInfo::startSearching() {
    searchData.nodes = 0;
    searchData.bestMove = Move::NO_MOVE;
    searchData.bestRootScore = -EVAL_INF;
    board = searcher->board;
    searchStack[STACK_OVERHEAD].accumulator->refresh(board);
    bucketCache = InputBucketCache();
    
    Search::iterativeDeepening(*this, searcher->limit,
                               searcher);

    if (type == ThreadType::MAIN) {
        searcher->stopSearching();
        searcher->waitForWorkersFinished();
        ThreadInfo* bestSearcher = this;
        searcher->TT.incAge();
        searcher->bestScore = bestSearcher->searchData.bestRootScore;
        if (searcher->printInfo)
            std::cout << "\nbestmove "
                      << uci::moveToUci(bestSearcher->searchData.bestMove,
                                        searcher->board.chess960())
                      << std::endl;
    }
}

void Search::ThreadInfo::waitForSearchFinished() {
    std::unique_lock<std::mutex> lock(mutex);
    cv.wait(lock, [&] { return !stops.searching; });
}

void Search::ThreadInfo::idle() {
    while (!stops.exiting) {
        std::unique_lock<std::mutex> lock(mutex);
        cv.wait(lock, [&] { return stops.searching; });

        if (stops.exiting)
            return;

        startSearching();
        stops.searching = false;

        cv.notify_all();
    }
}