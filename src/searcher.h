#pragma once

#include "external/chess.hpp"
#include "search.h"
#include "tt.h"
#include <atomic>
#include <barrier>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <thread>
#include <vector>

struct Searcher {
        TTable TT;
        std::vector<std::unique_ptr<Search::ThreadInfo>> threads;
        Search::Limit limit;
        Board board;

        std::shared_mutex mutex;
        std::unique_ptr<std::barrier<>> idle_barrier;
        std::unique_ptr<std::barrier<>> start_barrier;
        std::unique_ptr<std::barrier<>> stop_barrier;

        bool showWDL;
        bool printInfo = true;

        void initialize(int num) {
            if (threads.size() == num)
                return;

            {
                std::unique_lock guard{mutex};
                this->exit();
            }

            idle_barrier = std::make_unique<std::barrier<>>(num + 1);  // uci + search threads
            start_barrier = std::make_unique<std::barrier<>>(num + 1); // uci + search threads
            stop_barrier = std::make_unique<std::barrier<>>(num);      // search threads only

            for (size_t i = 0; i < num; i++) {
                threads.push_back(std::make_unique<Search::ThreadInfo>(i, TT, this));
            }
        }

        void startSearching(Board board, Search::Limit limit) {
            stopSearching();
            {
                std::unique_lock guard{mutex};
                this->board = board;
                this->limit = limit;
                for (auto& thread : threads) {
                    thread.get()->stopped.store(false);
                }
            }
            idle_barrier->arrive_and_wait();  // release search threads
            start_barrier->arrive_and_wait(); // halt uci thread until all search threads started
        }

        void exit() {
            stopSearching();
            for (auto& thread : threads) {
                thread->exiting.store(true);
            }
            if (idle_barrier) {
                idle_barrier->arrive_and_wait();
            }
            for (auto& thread : threads) {
                if (thread->thread.joinable())
                    thread->thread.join();
            }
            idle_barrier.reset();
            start_barrier.reset();
            stop_barrier.reset();
            threads.clear();
        }
        void stopSearching() {
            for (auto& thread : threads) {
                thread.get()->stopped.store(true);
            }
        }
        void wait() {
            // Wait to acquire exclusive access
            std::unique_lock guard{mutex};
        }
        void resizeTT(uint64_t size) {
            std::unique_lock guard{mutex};
            TT.resize(size);
            TT.clear(threads.size());
        }
        void reset() {
            std::unique_lock guard{mutex};
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
};
