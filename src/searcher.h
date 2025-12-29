#pragma once

#include "external/chess.hpp"
#include "search.h"
#include "tt.h"
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <vector>

struct Searcher {
        TTable TT;
        std::vector<std::unique_ptr<Search::ThreadInfo>> threads;
        Search::Limit limit;
        Board board;

        int bestScore = 0;

        bool showWDL;
        bool printInfo = true;
        bool useSoft = false;
        bool normalizeEval = true;

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
                thread.get()->stops.stopped.store(false);
                std::lock_guard<std::mutex> lock(thread.get()->mutex);
                thread.get()->stops.searching = true;
            }

            for (auto& thread : threads) {
                thread.get()->cv.notify_all();
            }
        }

        void exit() {
            for (auto& thread : threads)
                thread.get()->exit();
        }
        void stopSearching() {
            for (auto& thread : threads) {
                thread.get()->stops.stopped.store(true);
            }
        }
        void waitForSearchFinished() {
            for (auto& thread : threads) {
                thread.get()->waitForSearchFinished();
            }
        }
        void waitForWorkersFinished() {
            for (int i = 1; i < threads.size(); i++) {
                threads[i].get()->waitForSearchFinished();
            }
        }
        void resizeTT(uint64_t size) {
            TT.resize(size);
            TT.clear(threads.size());
        }
        void reset() {
            for (auto& thread : threads)
                thread.get()->reset();
            TT.clear(threads.size());
        }

        uint64_t nodeCount() {
            uint64_t nodes = 0;
            for (auto& thread : threads) {
                nodes += thread.get()->loadNodes();
            }
            return nodes;
        }

        void toggleWDL(bool x) {
            showWDL = x;
        }
        void toggleSoft(bool x) {
            useSoft = x;
        }
        void toggleNorm(bool x) {
            normalizeEval = x;
        }
};