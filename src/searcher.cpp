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
        searching.store(true);
        exiting.store(true);
    }
    cv.notify_all();
    if (thread.joinable())
        thread.join();
}

void Search::ThreadInfo::startSearching() {
    nodes = 0;
    bestMove = Move::NO_MOVE;
    bestRootScore = -EVAL_INF;
    
    Search::iterativeDeepening(searcher->board, *this, searcher->limit,
                               searcher);

    if (type == ThreadType::MAIN) {
        searcher->stopSearching();
        searcher->waitForWorkersFinished();
        ThreadInfo* bestSearcher = this;
        searcher->TT.incAge();
        searcher->bestScore = bestSearcher->bestRootScore;
        if (searcher->printInfo)
            std::cout << "\nbestmove "
                      << uci::moveToUci(bestSearcher->bestMove,
                                        searcher->board.chess960())
                      << std::endl;
    }
}

void Search::ThreadInfo::waitForSearchFinished() {
    std::unique_lock<std::mutex> lock(mutex);
    cv.wait(lock, [&] { return !searching.load(); });
}

void Search::ThreadInfo::idle() {
    while (!exiting.load()) {
        std::unique_lock<std::mutex> lock(mutex);
        cv.wait(lock, [&] { return searching.load(); });

        if (exiting.load())
            return;

        startSearching();
        searching.store(false);

        cv.notify_all();
    }
}