#pragma once

#include "external/chess.hpp"
#include "parameters.h"
#include <array>
#include <bit>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <sstream>
#include <vector>

// Based off SP
namespace Cuckoo {

	inline std::array<uint64_t, 8192> keyDiffs;
	inline std::array<chess::Move, 8192> moves;

	void init();
	constexpr uint64_t h1(uint64_t diff) {
		return diff % 8192;
	}
	constexpr uint64_t h2(uint64_t diff) {
		return (diff >> 16) % 8192;
	}
}