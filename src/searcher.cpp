#include "external/chess.hpp"
#include "tt.h"
#include "searcher.h"
#include <atomic>
#include <vector>
#include <thread>
#include <mutex>
#include <condition_variable>


Search::ThreadInfo::ThreadInfo(ThreadType t, TTable &tt, Searcher *s) : type(t), TT(tt), searcher(s) {
	board = Board();
	history.fill((int)DEFAULT_HISTORY);
	conthist.fill(DEFAULT_HISTORY);
	capthist.fill((int)DEFAULT_HISTORY);
	pawnCorrhist.fill((int)DEFAULT_HISTORY);
	nodes = 0;
	bestMove = Move::NO_MOVE;
	minNmpPly = 0;
	rootDepth = 0;
	thread = std::thread(&Search::ThreadInfo::idle, this);
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
	Search::iterativeDeepening(searcher->board, *this, searcher->limit, searcher);

	if (type == ThreadType::MAIN) {
		searcher->stopSearching();
		searcher->waitForWorkersFinished();

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