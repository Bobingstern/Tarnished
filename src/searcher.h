#pragma once

#include "external/chess.hpp"
#include "tt.h"
#include "search.h"
#include "threadpool.h"
#include <atomic>
#include <vector>
#include <thread>


struct Searcher {
	TTable TT;
	std::atomic<bool> abort;
	std::unique_ptr<Search::ThreadInfo> mainInfo = std::make_unique<Search::ThreadInfo>(ThreadType::MAIN, TT, abort);
	std::thread mainThread;

	std::vector<Search::ThreadInfo> workerInfo;
	std::unique_ptr<ThreadPool> pool;

	bool showWDL;
	

	void start(Board &board, Search::Limit limit);
	void stop();

	void initialize(int threads);

	void resizeTT(uint64_t size){
		TT.resize(size);
		TT.clear();
	}
	void reset(){
		mainInfo->reset();
		for (Search::ThreadInfo &w : workerInfo)
			w.reset();
		TT.clear();
	}

	uint64_t nodeCount(){
		uint64_t nodes = 0;
		for (Search::ThreadInfo &t : workerInfo){
			nodes += t.nodes;
		}
		return nodes + mainInfo->nodes;
	}

	void toggleWDL(bool x){
		showWDL = x;
	}
};