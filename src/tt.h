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


inline void* alignedAlloc(size_t alignment, size_t requiredBytes) {
    void* ptr;
#if defined(__MINGW32__)
    int offset = alignment - 1;
    void* p = (void*)malloc(requiredBytes + offset);
    ptr = (void*)(((size_t)(p)+offset) & ~(alignment - 1));
#elif defined (__GNUC__)
    ptr = std::aligned_alloc(alignment, requiredBytes);
#else
#error "Compiler not supported"
#endif

#if defined(__linux__)
    madvise(ptr, requiredBytes, MADV_HUGEPAGE);
#endif 

    return ptr;
}


using namespace chess;

enum TTFlag { NO_BOUND = 0, EXACT = 1, BETA_CUT = 2, FAIL_LOW = 3 };

// Heavily based off PlentyChess
constexpr int CLUSTER_SIZE = 3;

constexpr int GENERATION_PADDING = 3;
constexpr int GENERATION_DELTA = (1 << GENERATION_PADDING);
constexpr int GENERATION_CYCLE = 255 + GENERATION_DELTA;
constexpr int GENERATION_MASK = (0xFF << GENERATION_PADDING) & 0xFF;

extern uint8_t TT_GENERATION_COUNTER;

using ttkey = uint16_t;


struct TTEntry {
        ttkey zobrist;
        int16_t score;
        int16_t staticEval;
        uint16_t move;
        uint8_t depth;
        uint8_t flags;
        TTEntry() {
            this->zobrist = 0;
            this->move = 0;
            this->score = -32767;
            this->flags = 0;
            this->depth = 0;
            this->staticEval = -32767;
        }
        void updateEntry(uint64_t key, chess::Move best, int score, int eval, uint8_t flag, uint8_t depth, bool isPV) {
            ttkey keyShrink = static_cast<ttkey>(key);
            if (!moveIsNull(best) || keyShrink != this->zobrist)
                this->move = best.move();

            if (flag == TTFlag::EXACT || keyShrink != this->zobrist || depth + 2 * isPV + 4 > this->depth) {
                this->zobrist = keyShrink;
                this->score = static_cast<int16_t>(score);
                this->flags = uint8_t(flag + (isPV << 2)) | TT_GENERATION_COUNTER;
                this->depth = depth;
                this->staticEval = static_cast<int16_t>(eval);
            }
        }
        bool getPV() {
            return this->flags & 0x4;
        }
        uint8_t getFlag() {
            return this->flags & 0x3;
        }
};

struct TTCluster {
    TTEntry entries[CLUSTER_SIZE];
    char padding[2];
};

class TTable {
    TTCluster* table = nullptr;
    size_t clusters = 0;

public:
    TTable() {
        resize(16);
    }
    ~TTable() {
        std::free(table);
    }
    void newSearch() {
        TT_GENERATION_COUNTER += GENERATION_DELTA;
    }

    void resize(size_t mb) {
        size_t clusterCount = mb * 1024 * 1024 / sizeof(TTCluster);
        if (clusterCount == clusters)
            return;

        if (table)
            std::free(table);

        clusters = clusterCount;
        table = static_cast<TTCluster*>(alignedAlloc(sizeof(TTCluster), clusters * sizeof(TTCluster)));
    }
    size_t index(uint64_t key) {
        return static_cast<std::uint64_t>((static_cast<u128>(key) * static_cast<u128>(clusters)) >> 64);
    }

    TTEntry* probe(uint64_t key, bool &tthit) {
        TTCluster* cluster = &table[index(key)];
        uint16_t key16 = static_cast<ttkey>(key);

        TTEntry* replace = &cluster->entries[0];

        for (int i = 0; i < CLUSTER_SIZE; i++) {
            if (cluster->entries[i].zobrist == key16 || cluster->entries[i].zobrist == 0) {
                cluster->entries[i].flags = static_cast<uint8_t>( TT_GENERATION_COUNTER | (cluster->entries[i].flags & (GENERATION_DELTA - 1)) );
                tthit = cluster->entries[i].zobrist == key16;
                return &cluster->entries[i];
            }
            if (i > 0) {
                int replaceValue = replace->depth - ((GENERATION_CYCLE + TT_GENERATION_COUNTER - replace->flags) & GENERATION_MASK);
                int entryValue = cluster->entries[i].depth - ((GENERATION_CYCLE + TT_GENERATION_COUNTER - cluster->entries[i].flags) & GENERATION_MASK);
                if ((cluster->entries[i].zobrist == 0 && replace->zobrist != 0) || replaceValue > entryValue)
                    replace = &cluster->entries[i];
            }
        }
        tthit = false;
        return replace;
    }

    int hashfull() {
        int count = 0;
        for (int i = 0; i < 1000; i++) {
            for (int j = 0; j < CLUSTER_SIZE; j++) {
                if ((table[i].entries[j].flags & GENERATION_MASK) == TT_GENERATION_COUNTER && table[i].entries[j].zobrist != 0)
                    count++;
            }
        }
        return count / CLUSTER_SIZE;
    }
    void clear(int threadCount = 1) {
        std::vector<std::thread> ts;

        for (size_t thread = 0; thread < threadCount; thread++) {
            size_t startCluster = thread * (clusters / threadCount);
            size_t endCluster = thread == threadCount - 1 ? clusters : (thread + 1) * (clusters / threadCount);
            ts.push_back(std::thread([this, startCluster, endCluster]() {
                std::memset(static_cast<void*>(&table[startCluster]), 0, sizeof(TTCluster) * (endCluster - startCluster));
            }));
        }

        for (auto& t : ts) {
            t.join();
        }

        TT_GENERATION_COUNTER = 0;
    }
};