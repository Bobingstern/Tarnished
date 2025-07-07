#pragma once

#include "external/chess.hpp"
#include "eval.h"
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

// Heavily based off Stormphrax and Sirius

constexpr int ENTRY_COUNT = 3;
constexpr size_t TT_ALIGNMENT = 64;
static constexpr int GEN_CYCLE_LENGTH = 1 << 5;


inline int storeScore(int score, int ply) {
    if (std::abs(score) >= FOUND_MATE)
        score += score < 0 ? -ply : ply;
    return score;
}
inline int readScore(int score, int ply) {
    if (std::abs(score) >= FOUND_MATE)
        score += score < 0 ? ply : -ply;
    return score;
}

struct TTEntry {
    uint16_t key16;
    int16_t score;
    int16_t staticEval;
    uint16_t bestMove;
    uint8_t depth;
    uint8_t flags;

    bool pv() { return (flags >> 2) & 1; }

    uint8_t gen() { return flags >> 3; }

    uint8_t bound() { return flags & 3; }

    void setFlag(bool pv, uint8_t gen, uint8_t bound) {
        flags = bound | (pv << 2) | (gen << 3);
    }
};

struct ProbedTTEntry {
    int score;
    int staticEval;
    uint16_t move;
    int depth;
    bool pv;
    uint8_t bound;
};

struct alignas(32) TTCluster {
    TTEntry entries[ENTRY_COUNT];
    char padding[2];
};

class TTable {

public:
    TTable() {
        resize(16);
    }
    ~TTable() {
        std::free(clusters);
    }

    void resize(int mb = 16) {
        size_t clusterCount = static_cast<uint64_t>(mb) * 1024 * 1024 / sizeof(TTCluster);
        if (clusterCount == size)
            return;

        std::free(clusters);

        size = clusterCount;
        clusters = static_cast<TTCluster*>(alignedAlloc(TT_ALIGNMENT, size * sizeof(TTCluster)));
        currAge = 0;
    }

    bool probe(uint64_t key, int ply, ProbedTTEntry& ttData) {
        size_t idx = index(key);
        TTCluster& cluster = clusters[idx];
        int entryIdx = -1;
        uint16_t key16 = key & 0xFFFF;

        for (int i = 0; i < ENTRY_COUNT; i++) {
            if (cluster.entries[i].key16 == key16) {
                entryIdx = i;
                break;
            }
        }

        if (entryIdx == -1)
            return false;

        auto entry = cluster.entries[entryIdx];

        ttData.score = readScore(entry.score, ply);
        ttData.staticEval = entry.staticEval;
        ttData.move = entry.bestMove;
        ttData.depth = entry.depth;
        ttData.bound = entry.bound();
        ttData.pv = entry.pv();

        return true;
    }

    void store(uint64_t key, Move move, int score, int staticEval, uint8_t bound, int depth, int ply, bool pv) {
        uint16_t key16 = key & 0xFFFF;
        TTCluster& cluster = clusters[index(key)];
        int currQuality = INT_MAX;
        int replaceIdx = -1;
        for (int i = 0; i < ENTRY_COUNT; i++) {
            if (cluster.entries[i].key16 == key16) {
                replaceIdx = i;
                break;
            }

            int entryQuality = quality(cluster.entries[i].gen(), cluster.entries[i].depth);
            if (entryQuality < currQuality) {
                currQuality = entryQuality;
                replaceIdx = i;
            }
        }

        TTEntry& replace = cluster.entries[replaceIdx];
        if (!moveIsNull(move) || replace.key16 != key16)
            replace.bestMove = move.move();

        if (bound == TTFlag::EXACT || replace.key16 != key16 || depth >= replace.depth - 4 - 2 * pv || replace.gen() != currAge) {
            replace.key16 = key16;
            replace.staticEval = staticEval;
            replace.depth = static_cast<uint8_t>(depth);
            replace.score = static_cast<int16_t>(storeScore(score, ply));
            replace.setFlag(pv, static_cast<uint8_t>(currAge), bound);
        }
    }

    int quality(int age, int depth) {
        int ageDiff = currAge - age;
        if (ageDiff < 0) {
            ageDiff += GEN_CYCLE_LENGTH;
        }
        return depth - 2 * ageDiff;
    }

    void incAge() {
        currAge = (currAge + 1) % GEN_CYCLE_LENGTH;
    }
    void clear(int numThreads = 1) {
        currAge = 0;
        std::vector<std::jthread> threads;
        threads.reserve(numThreads);
        for (int i = 0; i < numThreads; i++) {
            threads.emplace_back(
                [i, this, numThreads]() {
                    auto begin = clusters + size * i / numThreads;
                    auto end = clusters + size * (i + 1) / numThreads;
                    std::fill(begin, end, TTCluster{});
                });
        }
    }

    int hashfull() {
        int count = 0;
        for (int i = 0; i < 1000; i++) {
            for (int j = 0; j < ENTRY_COUNT; j++) {
                auto& entry = clusters[i].entries[j];
                if (entry.bound() != TTFlag::NO_BOUND && entry.gen() == currAge)
                    count++;
            }
        }
        return count / ENTRY_COUNT;
    }

private:
    TTCluster* clusters;
    size_t size;
    int currAge;
    uint32_t index(uint64_t key) {
        return static_cast<std::uint64_t>((static_cast<u128>(key) * static_cast<u128>(size)) >> 64);
    }
};