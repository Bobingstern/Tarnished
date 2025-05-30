#pragma once

#include "external/chess.hpp"
#include "parameters.h"
#include <vector>
#include <thread>
#include <bitset>
#include <climits>
#include <cstring>
#include <iostream>

using namespace chess;

enum TTFlag {
	EXACT = 1,
	BETA_CUT = 2,
	FAIL_LOW = 3
};

struct TTEntry {
	uint64_t zobrist;
	chess::Move move;
	int score;
	uint8_t flag;
	uint8_t depth;
	TTEntry(){
		this->zobrist = 0;
		this->move = chess::Move();
		this->score = 0;
		this->flag = 0;
		this->depth = 0;
	}
	TTEntry(uint64_t key, chess::Move best, int score, uint8_t flag, uint8_t depth){
		this->zobrist = key;
		this->move = best;
		this->score = score;
		this->flag = flag;
		this->depth = depth;
	}
};

struct TTable {
private:
	TTEntry *table;
public:
	uint64_t size;

	TTable(uint64_t sizeMB = 16){
		table = nullptr;
		resize(sizeMB);
	}
	void clear(size_t threadCount = 1){
		std::vector<std::thread> threads;

		auto clearTT = [&](size_t threadId) {
			// The segment length is the number of entries each thread must clear
			// To find where your thread should start (in entries), you can do threadId * segmentLength
			// Converting segment length into the number of entries to clear can be done via length * bytes per entry

			size_t start = (size * threadId) / threadCount;
			size_t end   = std::min((size * (threadId + 1)) / threadCount, size);

			std::memset(table + start, 0, (end - start) * sizeof(TTEntry));
		};

		for (size_t thread = 1; thread < threadCount; thread++)
			threads.emplace_back(clearTT, thread);

		clearTT(0);

		for (std::thread& t : threads)
			if (t.joinable())
				t.join();
	}
	void resize(uint64_t MB){
		size = MB * 1024 * 1024 / sizeof(TTEntry);
		if (table != nullptr)
			std::free(table);
		table = static_cast<TTEntry*>(std::malloc(size * sizeof(TTEntry)));
	}
	uint64_t index(uint64_t key) { 
		return static_cast<std::uint64_t>((static_cast<u128>(key) * static_cast<u128>(size)) >> 64);
	}

	TTEntry *getEntry(uint64_t key){
		return &table[index(key)];
	}

	size_t hashfull() {
		size_t samples = std::min((uint64_t) 1000, size);
		size_t hits	= 0;
		for (size_t sample = 0; sample < samples; sample++)
			hits += table[sample].zobrist != 0;
		size_t hash = (int) (hits / (double) samples * 1000);
		assert(hash <= 1000);
		return hash;
	}

};