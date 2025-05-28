#pragma once

#include "external/chess.hpp"
#include <vector>

using namespace chess;

enum TTFlag {
	EXACT = 1,
	BETA_CUT = 2,
	FAIL_LOW = 3
};

struct TTEntry {
	uint64_t zobrist;
	int score;
	uint16_t move;
	uint8_t flag;
	uint8_t depth;
	TTEntry(){
		this->zobrist = 0;
		this->move = 0;
		this->score = 0;
		this->flag = 0;
		this->depth = 0;
	}
	TTEntry(uint64_t key, uint16_t best, int score, uint8_t flag, uint8_t depth){
		this->zobrist = key;
		this->move = best;
		this->score = score;
		this->flag = flag;
		this->depth = depth;
	}
};

struct TTable {
private:
	std::vector<TTEntry> table;
public:
	uint64_t size;

	TTable(uint64_t sizeMB = 16){

		resize(sizeMB);
	}
	void clear(){
		std::fill(table.begin(), table.end(), TTEntry{});
	}
	void resize(uint64_t MB){
		size = MB * 1024 * 1024 / sizeof(TTEntry);
		if (size == 0)
			size++;
		table.resize(size);
	}
	uint64_t index(uint64_t key) { 
		return key % size;
	}

	void setEntry(TTEntry entry){
		table[index(entry.zobrist)] = entry;
	}
	TTEntry *getEntry(uint64_t key){
		return &table[index(key)];
	}

};