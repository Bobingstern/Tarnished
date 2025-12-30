#pragma once

#include "external/chess.hpp"
#include "search.h"
#include "tt.h"
#include <atomic>
#include <barrier>
#include <condition_variable>
#include <shared_mutex>
#include <thread>
#include <vector>

struct Searcher {
        TTable TT;
        std::vector<std::unique_ptr<Search::ThreadInfo>> threads;
        using BarrierPtr = std::unique_ptr<std::barrier<>>;
        std::shared_mutex mutex;
        BarrierPtr idleBarrier;
        BarrierPtr startedBarrier;

        Search::Limit limit;
        Board board;

        int bestScore = 0;

        bool showWDL;
        bool printInfo = true;
        bool useSoft = false;
        bool normalizeEval = true;

        Searcher() {
            idleBarrier = std::make_unique<std::barrier<>>(1);
            startedBarrier = std::make_unique<std::barrier<>>(1);
        }
        void initialize(int num) {
            if (threads.size() == num)
                return;
            {
                std::unique_lock lockGuard{mutex};
                for (auto& thread : threads) {
                    thread->exit();
                }
                idleBarrier->arrive_and_wait();
                threads.clear();
            }
            idleBarrier = std::make_unique<std::barrier<>>(1 + num);
            startedBarrier = std::make_unique<std::barrier<>>(1 + num);

            if (num > 0) {
                threads.push_back(std::make_unique<Search::ThreadInfo>(0, *this));
                for (size_t i = 1; i < num; i++) {
                    threads.push_back(std::make_unique<Search::ThreadInfo>(i, *this));
                }
            }
        }

        void startSearching(Board board, Search::Limit limit) {
            
            {
                std::unique_lock lockGuard{mutex};
                this->board = board;
                this->limit = limit;
                TT.incAge();
                for (auto& thread : threads) {
                    thread->prepare();
                }
            }
            idleBarrier->arrive_and_wait();
            startedBarrier->arrive_and_wait();
        }

        void exit() {
            initialize(0);
        }
        void stopSearching() {
            for (auto& thread : threads) {
                thread.get()->setStopped();
            }
        }
        void waitForSearchFinished() {
            std::unique_lock lock{mutex};
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
            std::unique_lock lockGuard{mutex};
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