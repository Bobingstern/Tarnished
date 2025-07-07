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
        searching = true;
        exiting = true;
    }
    cv.notify_all();
    if (thread.joinable())
        thread.join();
}

void Search::ThreadInfo::startSearching() {
    nodes = 0;
    bestMove = Move::NO_MOVE;
    bestRootScore = -INFINITE;
    
    Search::iterativeDeepening(searcher->board, *this, searcher->limit,
                               searcher);

    if (type == ThreadType::MAIN) {
        searcher->stopSearching();
        searcher->waitForWorkersFinished();
        ThreadInfo* bestSearcher = this;
        for (auto& thread : searcher->threads) {
            if (thread.get()->type == ThreadType::MAIN)
                continue;

            // This should never happen but saftey
            if (!isLegal(searcher->board, thread.get()->bestMove))
                continue;

            int bestDepth = bestSearcher->completed;
            int bestScore = bestSearcher->bestRootScore;
            int currentDepth = thread->completed;
            int currentScore = thread->bestRootScore;
            if ((bestDepth == currentDepth && currentScore > bestScore) ||
                (Search::isWin(currentScore) && currentScore > bestScore))
                bestSearcher = thread.get();
            if (currentDepth > bestDepth &&
                (currentScore > bestScore || !Search::isWin(bestScore)))
                bestSearcher = thread.get();
        }
        searcher->TT.incAge();
        std::cout << "\nbestmove "
                  << uci::moveToUci(bestSearcher->bestMove,
                                    searcher->board.chess960())
                  << std::endl;
    }
}

void Search::ThreadInfo::waitForSearchFinished() {
    std::unique_lock<std::mutex> lock(mutex);
    cv.wait(lock, [&] { return !searching; });
}

void Search::ThreadInfo::idle() {
    while (!exiting) {
        std::unique_lock<std::mutex> lock(mutex);
        cv.wait(lock, [&] { return searching; });

        if (exiting)
            return;

        startSearching();
        searching = false;

        cv.notify_all();
    }
}