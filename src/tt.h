#pragma once

#include "external/chess.hpp"
#include "parameters.h"
#include "util.h"
#include <bitset>
#include <climits>
#include <cstring>
#include <iostream>
#include <thread>
#include <vector>
#if defined(__linux__)
    #include <sys/mman.h>
#endif

using namespace chess;

enum TTFlag { NO_BOUND = 0, EXACT = 1, BETA_CUT = 2, FAIL_LOW = 3 };

using ttkey = uint16_t;
struct TTEntry {
        ttkey zobrist;
        int16_t score;
        int16_t staticEval;
        uint16_t move;
        uint8_t flag;
        uint8_t depth;
        bool isPV;
        TTEntry() {
            this->zobrist = 0;
            this->move = 0;
            this->score = -32767;
            this->flag = 0;
            this->depth = 0;
            this->staticEval = -32767;
            this->isPV = false;
        }
        TTEntry(uint64_t key, chess::Move best, int16_t score, int16_t eval, uint8_t flag, uint8_t depth, bool isPV) {
            this->move = best.move();
            this->zobrist = static_cast<ttkey>(key);
            this->score = score;
            this->flag = flag;
            this->depth = depth;
            this->staticEval = eval;
            this->isPV = isPV;
        }
        void updateEntry(uint64_t key, chess::Move best, int score, int eval, uint8_t flag, uint8_t depth, bool isPV) {
            ttkey keyShrink = static_cast<ttkey>(key);
            if (!moveIsNull(best) || keyShrink != this->zobrist)
                this->move = best.move();
            this->zobrist = keyShrink;
            this->score = static_cast<int16_t>(score);
            this->flag = flag;
            this->depth = depth;
            this->staticEval = static_cast<int16_t>(eval);
            this->isPV = isPV;
        }
};

struct TTable {
    private:
        TTEntry* table;

    public:
        uint64_t size;

        TTable(uint64_t sizeMB = 16) {
            table = nullptr;
            resize(sizeMB);
        }
        void clear(size_t threadCount = 1) {
            std::vector<std::thread> threads;

            auto clearTT = [&](size_t threadId) {
                // The segment length is the number of entries each thread must clear
                // To find where your thread should start (in entries), you can do threadId * segmentLength
                // Converting segment length into the number of entries to clear can be done via length * bytes per
                // entry

                size_t start = (size * threadId) / threadCount;
                size_t end = std::min((size * (threadId + 1)) / threadCount, size);

                std::memset(table + start, 0, (end - start) * sizeof(TTEntry));
            };

            for (size_t thread = 1; thread < threadCount; thread++)
                threads.emplace_back(clearTT, thread);

            clearTT(0);

            for (std::thread& t : threads)
                if (t.joinable())
                    t.join();
        }
        void resize(uint64_t MB) {
            size = MB * 1024 * 1024 / sizeof(TTEntry);
            if (table != nullptr)
                std::free(table);
            table = static_cast<TTEntry*>(std::malloc(size * sizeof(TTEntry)));
        }
        uint64_t index(uint64_t key) {
            // return key % size;
            return static_cast<std::uint64_t>((static_cast<u128>(key) * static_cast<u128>(size)) >> 64);
        }

        TTEntry* getEntry(uint64_t key) {
            return &table[index(key)];
        }

        TTEntry getEntryCopy(uint64_t key) {
            return table[index(key)];
        }

        size_t hashfull() {
            size_t samples = std::min((uint64_t)1000, size);
            size_t hits = 0;
            for (size_t sample = 0; sample < samples; sample++)
                hits += table[sample].zobrist != 0;
            size_t hash = (int)(hits / (double)samples * 1000);
            assert(hash <= 1000);
            return hash;
        }
};