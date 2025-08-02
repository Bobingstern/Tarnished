#pragma once

#include "external/chess.hpp"
#include "search.h"
#include "tt.h"
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <shared_mutex>
#include <barrier>
#include <thread>
#include <vector>

struct Searcher {
        TTable TT;
        std::vector<std::unique_ptr<Search::ThreadInfo>> threads;
        Search::Limit limit;
        Board board;

        bool showWDL;
        bool printInfo = true;

        std::shared_mutex mutex;
        std::unique_ptr<std::barrier<>> idleBarrier;
        std::unique_ptr<std::barrier<>> startedBarrier;


        Searcher() : 
            idleBarrier(std::make_unique<std::barrier<>>(1)), 
            startedBarrier(std::make_unique<std::barrier<>>(1)) {

        }

        void initialize(int num) {
            if (threads.size() == num)
                return;

            {
                std::unique_lock lock_guard{mutex};
                for (auto& thread : threads) {
                    thread->exit();
                }
                idleBarrier->arrive_and_wait();
                threads.clear();
            }

            idleBarrier = std::make_unique<std::barrier<>>(1 + num);
            startedBarrier = std::make_unique<std::barrier<>>(1 + num);

            threads.push_back(std::make_unique<Search::ThreadInfo>(0, TT, this));
            for (size_t i = 1; i < num; i++) {
                threads.push_back(std::make_unique<Search::ThreadInfo>(i, TT, this));
            }
        }

        void startSearching(Board board, Search::Limit limit) {
            {
                std::unique_lock lock_guard{mutex};
                this->board = board;
                this->limit = limit;
                for (auto& thread : threads) {
                    thread->stopped.store(false);
                }
            }
            idleBarrier->arrive_and_wait();
            startedBarrier->arrive_and_wait();

            // stopSearching();
            // waitForSearchFinished();
            // this->board = board;
            // this->limit = limit;
            // for (auto& thread : threads) {
            //     thread.get()->stopped.store(false);
            //     std::lock_guard<std::mutex> lock(thread.get()->mutex);
            //     thread.get()->searching.store(true);
            // }

            // for (auto& thread : threads) {
            //     thread.get()->cv.notify_all();
            // }
        }

        void exit() {
            for (auto& thread : threads)
                thread.get()->exit();
            initialize(0);
        }
        void stopSearching() {
            for (auto& thread : threads) {
                thread.get()->stopped.store(true);
            }
        }
        void wait() {
            std::unique_lock lock_guard{mutex};
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
};