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
	thread = std::thread(&Search::ThreadInfo::idle, this);
	reset();
};

Search::ThreadInfo::ThreadInfo(int id, TTable &tt, Searcher *s) : threadId(id), TT(tt), searcher(s) {
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

	Search::iterativeDeepening(searcher->board, *this, searcher->limit, searcher);

	if (type == ThreadType::MAIN) {
		searcher->stopSearching();
		searcher->waitForWorkersFinished();
		//std::cout << "\nbestmove " << uci::moveToUci(bestMove) << std::endl;
		ThreadInfo *bestSearcher = this;
		for (auto &thread : searcher->threads) {
			if (thread.get()->type == ThreadType::MAIN)
				continue;
			int bestDepth = bestSearcher->completed;
			int bestScore = bestSearcher->threadBestScore;
			int currentDepth = thread->completed;
			int currentScore = thread->threadBestScore;
			if ( (bestDepth == currentDepth && currentScore > bestScore) || (Search::isMateScore(currentScore) && currentScore > bestScore))
				bestSearcher = thread.get();
			if (currentDepth > bestDepth && (currentScore > bestScore || !Search::isMateScore(bestScore)))
				bestSearcher = thread.get();
		}
		std::cout << "\nbestmove " << uci::moveToUci(bestSearcher->bestMove, searcher->board.chess960()) << std::endl;
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