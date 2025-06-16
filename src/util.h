#pragma once

#include "external/chess.hpp"
#include "nnue.h"
#include <bit>
#include <vector>
#include <sstream>
#include <cassert>
#include <cstring>
#include <cstdint>

using namespace chess;

enum DirIndex {
	NORTH = 0,
	SOUTH = 1,
	EAST = 2,
	WEST = 3,
	NORTH_EAST = 4,
	NORTH_WEST = 5,
	SOUTH_EAST = 6,
	SOUTH_WEST = 7
};

struct StateInfo {
	Bitboard pinners[2];
	Bitboard kingBlockers[2];
	StateInfo(){
		pinners[0] = Bitboard(0); pinners[1] = Bitboard(0);
		kingBlockers[0] = Bitboard(0); kingBlockers[1] = Bitboard(0);
	}
};
// Values taken from SF
constexpr int PawnValue   = 100;
constexpr int KnightValue = 316;
constexpr int BishopValue = 328;
constexpr int RookValue   = 493;
constexpr int QueenValue  = 982;

inline std::array<int, 8> PieceValue = {PawnValue, KnightValue, BishopValue, RookValue, QueenValue, 0, 0};

// [stm][side]
// kingside is 0, queenside 1
inline std::array<std::array<Square, 2>, 2> castleRookSq = {{
    {{Square::SQ_H1, Square::SQ_A1}},
    {{Square::SQ_H8, Square::SQ_A8}}
}};

template<typename T, typename U>
inline void deepFill(T& dest, const U& val) {
    dest = val;
}

template<typename T, size_t N, typename U>
inline void deepFill(std::array<T, N>& arr, const U& value) {
    for (auto& element : arr) {
        deepFill(element, value);
    }
}
namespace Search {
    struct Stack;
};

// Hash Keys
uint64_t resetPawnHash(Board &board);
uint64_t resetNonPawnHash(Board &board, Color c);
uint64_t resetMajorHash(Board &board);
uint64_t resetMinorHash(Board &board);
bool isMajor(PieceType pt);
bool isMinor(PieceType pt);

// Legality
bool isLegal(Board &board, Move move);

// Accumulator wrapper
void MakeMove(Board &board, Move move, Search::Stack *ss);
void UnmakeMove(Board &board, Move move);
// SEE stuff
void initLookups();
int oppDir(int dir);
Bitboard attackersTo(Board &board, Square s, Bitboard occ);
void pinnersBlockers(Board &board, Color c, StateInfo sti);
bool SEE(Board &board, Move &move, int margin);

// Util Move
static bool moveIsNull(Move m){
	return m == Move::NO_MOVE;
}


// Murmur hash
// sirius yoink
constexpr uint64_t murmurHash3(uint64_t key)
{
    key ^= key >> 33;
    key *= 0xff51afd7ed558ccd;
    key ^= key >> 33;
    key *= 0xc4ceb9fe1a85ec53;
    key ^= key >> 33;
    return key;
};

// Thanks shawn and stockfish
template<typename T, std::size_t Size, std::size_t... Sizes>
class MultiArray;

namespace Detail {

template<typename T, std::size_t Size, std::size_t... Sizes>
struct MultiArrayHelper {
    using ChildType = MultiArray<T, Sizes...>;
};

template<typename T, std::size_t Size>
struct MultiArrayHelper<T, Size> {
    using ChildType = T;
};

template<typename To, typename From>
constexpr bool is_strictly_assignable_v =
  std::is_assignable_v<To&, From> && (std::is_same_v<To, From> || !std::is_convertible_v<From, To>);

}

// MultiArray is a generic N-dimensional array.
// The template parameters (Size and Sizes) encode the dimensions of the array.
template<typename T, std::size_t Size, std::size_t... Sizes>
class MultiArray {
    using ChildType = typename Detail::MultiArrayHelper<T, Size, Sizes...>::ChildType;
    using ArrayType = std::array<ChildType, Size>;
    ArrayType data_;

   public:
    using value_type             = typename ArrayType::value_type;
    using size_type              = typename ArrayType::size_type;
    using difference_type        = typename ArrayType::difference_type;
    using reference              = typename ArrayType::reference;
    using const_reference        = typename ArrayType::const_reference;
    using pointer                = typename ArrayType::pointer;
    using const_pointer          = typename ArrayType::const_pointer;
    using iterator               = typename ArrayType::iterator;
    using const_iterator         = typename ArrayType::const_iterator;
    using reverse_iterator       = typename ArrayType::reverse_iterator;
    using const_reverse_iterator = typename ArrayType::const_reverse_iterator;

    constexpr auto&       at(size_type index) noexcept { return data_.at(index); }
    constexpr const auto& at(size_type index) const noexcept { return data_.at(index); }

    constexpr auto&       operator[](size_type index) noexcept { return data_[index]; }
    constexpr const auto& operator[](size_type index) const noexcept { return data_[index]; }

    constexpr auto&       front() noexcept { return data_.front(); }
    constexpr const auto& front() const noexcept { return data_.front(); }
    constexpr auto&       back() noexcept { return data_.back(); }
    constexpr const auto& back() const noexcept { return data_.back(); }

    auto*       data() { return data_.data(); }
    const auto* data() const { return data_.data(); }

    constexpr auto begin() noexcept { return data_.begin(); }
    constexpr auto end() noexcept { return data_.end(); }
    constexpr auto begin() const noexcept { return data_.begin(); }
    constexpr auto end() const noexcept { return data_.end(); }
    constexpr auto cbegin() const noexcept { return data_.cbegin(); }
    constexpr auto cend() const noexcept { return data_.cend(); }

    constexpr auto rbegin() noexcept { return data_.rbegin(); }
    constexpr auto rend() noexcept { return data_.rend(); }
    constexpr auto rbegin() const noexcept { return data_.rbegin(); }
    constexpr auto rend() const noexcept { return data_.rend(); }
    constexpr auto crbegin() const noexcept { return data_.crbegin(); }
    constexpr auto crend() const noexcept { return data_.crend(); }

    constexpr bool      empty() const noexcept { return data_.empty(); }
    constexpr size_type size() const noexcept { return data_.size(); }
    constexpr size_type max_size() const noexcept { return data_.max_size(); }

    template<typename U>
    void fill(const U& v) {
        static_assert(Detail::is_strictly_assignable_v<T, U>,
                      "Cannot assign fill value to entry type");
        for (auto& ele : data_)
        {
            if constexpr (sizeof...(Sizes) == 0)
                ele = v;
            else
                ele.fill(v);
        }
    }

    constexpr void swap(MultiArray<T, Size, Sizes...>& other) noexcept { data_.swap(other.data_); }
};
