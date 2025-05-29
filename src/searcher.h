#pragma once

#include "external/chess.hpp"
#include "tt.h"
#include "search.h"
#include <atomic>
#include <vector>
#include <thread>
#include <mutex>
#include <condition_variable>

struct Searcher {
	TTable TT;
	std::vector<std::unique_ptr<Search::ThreadInfo>> threads;
	Search::Limit limit;
	Board board;

	bool showWDL;
	bool printInfo = true;
	

	void start(Board &board, Search::Limit limit);
	void stop();

	void initialize(int num) {
		if (threads.size() == num)
			return;
		this->exit();
		threads.clear();
		threads.push_back(std::make_unique<Search::ThreadInfo>(0, TT, this));
        for (size_t i = 1; i < num; i++) {
            threads.push_back(std::make_unique<Search::ThreadInfo>(i, TT, this));
        }
	}

	void startSearching(Board board, Search::Limit limit) {
		stopSearching();
		waitForSearchFinished();
		this->board = board;
		this->limit = limit;
		for (auto& thread : threads) {
            thread.get()->stopped = false;
            std::lock_guard<std::mutex> lock(thread.get()->mutex);
            thread.get()->searching = true;
        }

        for (auto& thread : threads) {
            thread.get()->cv.notify_all();
        }
	}

	void exit(){
		for (auto &thread : threads)
			thread.get()->exit();
	}
	void stopSearching() {
		for (auto &thread : threads) {
			thread.get()->stopped = true;
		}
	}
	void waitForSearchFinished() {
		for (auto &thread : threads) {
			thread.get()->waitForSearchFinished();
		}
	}
	void waitForWorkersFinished() {
		for (int i = 1; i < threads.size(); i++) {
            threads[i].get()->waitForSearchFinished();
        }
	}
	void resizeTT(uint64_t size){
		TT.resize(size);
		TT.clear(threads.size());
	}
	void reset(){
		for (auto &thread : threads)
			thread.get()->reset();
		TT.clear(threads.size());
	}

	uint64_t nodeCount(){
		uint64_t nodes = 0;
		for (auto &thread : threads){
			nodes += thread.get()->nodes;
		}
		return nodes;
	}

	void toggleWDL(bool x){
		showWDL = x;
	}
};