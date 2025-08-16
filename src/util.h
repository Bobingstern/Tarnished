#pragma once

#include "external/chess.hpp"
#include "external/fmt/color.h"
#include "external/fmt/format.h"
#include "nnue.h"
#include "parameters.h"
#include <bit>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <sstream>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/ioctl.h>
#include <unistd.h>
#endif

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
        StateInfo() {
            pinners[0] = Bitboard(0);
            pinners[1] = Bitboard(0);
            kingBlockers[0] = Bitboard(0);
            kingBlockers[1] = Bitboard(0);
        }
};

extern std::array<int, 8> PieceValue;

// [stm][side]
// kingside is 0, queenside 1
inline std::array<std::array<Square, 2>, 2> castleRookSq = {
    {{{Square::SQ_H1, Square::SQ_A1}}, {{Square::SQ_H8, Square::SQ_A8}}}};

template <typename T, typename U> inline void deepFill(T& dest, const U& val) {
    dest = val;
}

template <typename T, size_t N, typename U> inline void deepFill(std::array<T, N>& arr, const U& value) {
    for (auto& element : arr) {
        deepFill(element, value);
    }
}
namespace Search {
    struct Stack;
};

// Hash Keys
uint64_t resetPawnHash(Board& board);
uint64_t resetNonPawnHash(Board& board, Color c);
uint64_t resetMajorHash(Board& board);
uint64_t resetMinorHash(Board& board);
bool isMajor(PieceType pt);
bool isMinor(PieceType pt);

// Legality
bool isLegal(Board& board, Move move);

// Threats
std::array<Bitboard, 7> calculateThreats(Board& board);

// Accumulator wrapper
void MakeMove(Board& board, Move move, Search::Stack* ss);
void UnmakeMove(Board& board, Move move);
// SEE stuff
void initLookups();
int oppDir(int dir);
Bitboard attackersTo(Board& board, Square s, Bitboard occ);
void pinnersBlockers(Board& board, Color c, StateInfo sti);
bool SEE(Board& board, Move& move, int margin);

// Util Move
static bool moveIsNull(Move m) {
    return m == Move::NO_MOVE;
}

// Murmur hash
// sirius yoink
constexpr uint64_t murmurHash3(uint64_t key) {
    key ^= key >> 33;
    key *= 0xff51afd7ed558ccd;
    key ^= key >> 33;
    key *= 0xc4ceb9fe1a85ec53;
    key ^= key >> 33;
    return key;
};

// Thanks shawn and stockfish
template <typename T, std::size_t Size, std::size_t... Sizes> class MultiArray;

namespace Detail {

    template <typename T, std::size_t Size, std::size_t... Sizes> struct MultiArrayHelper {
            using ChildType = MultiArray<T, Sizes...>;
    };

    template <typename T, std::size_t Size> struct MultiArrayHelper<T, Size> {
            using ChildType = T;
    };

    template <typename To, typename From>
    constexpr bool is_strictly_assignable_v =
        std::is_assignable_v<To&, From> && (std::is_same_v<To, From> || !std::is_convertible_v<From, To>);

}

// MultiArray is a generic N-dimensional array.
// The template parameters (Size and Sizes) encode the dimensions of the array.
template <typename T, std::size_t Size, std::size_t... Sizes> class MultiArray {
        using ChildType = typename Detail::MultiArrayHelper<T, Size, Sizes...>::ChildType;
        using ArrayType = std::array<ChildType, Size>;
        ArrayType data_;

    public:
        using value_type = typename ArrayType::value_type;
        using size_type = typename ArrayType::size_type;
        using difference_type = typename ArrayType::difference_type;
        using reference = typename ArrayType::reference;
        using const_reference = typename ArrayType::const_reference;
        using pointer = typename ArrayType::pointer;
        using const_pointer = typename ArrayType::const_pointer;
        using iterator = typename ArrayType::iterator;
        using const_iterator = typename ArrayType::const_iterator;
        using reverse_iterator = typename ArrayType::reverse_iterator;
        using const_reverse_iterator = typename ArrayType::const_reverse_iterator;

        constexpr auto& at(size_type index) noexcept {
            return data_.at(index);
        }
        constexpr const auto& at(size_type index) const noexcept {
            return data_.at(index);
        }

        constexpr auto& operator[](size_type index) noexcept {
            return data_[index];
        }
        constexpr const auto& operator[](size_type index) const noexcept {
            return data_[index];
        }

        constexpr auto& front() noexcept {
            return data_.front();
        }
        constexpr const auto& front() const noexcept {
            return data_.front();
        }
        constexpr auto& back() noexcept {
            return data_.back();
        }
        constexpr const auto& back() const noexcept {
            return data_.back();
        }

        auto* data() {
            return data_.data();
        }
        const auto* data() const {
            return data_.data();
        }

        constexpr auto begin() noexcept {
            return data_.begin();
        }
        constexpr auto end() noexcept {
            return data_.end();
        }
        constexpr auto begin() const noexcept {
            return data_.begin();
        }
        constexpr auto end() const noexcept {
            return data_.end();
        }
        constexpr auto cbegin() const noexcept {
            return data_.cbegin();
        }
        constexpr auto cend() const noexcept {
            return data_.cend();
        }

        constexpr auto rbegin() noexcept {
            return data_.rbegin();
        }
        constexpr auto rend() noexcept {
            return data_.rend();
        }
        constexpr auto rbegin() const noexcept {
            return data_.rbegin();
        }
        constexpr auto rend() const noexcept {
            return data_.rend();
        }
        constexpr auto crbegin() const noexcept {
            return data_.crbegin();
        }
        constexpr auto crend() const noexcept {
            return data_.crend();
        }

        constexpr bool empty() const noexcept {
            return data_.empty();
        }
        constexpr size_type size() const noexcept {
            return data_.size();
        }
        constexpr size_type max_size() const noexcept {
            return data_.max_size();
        }

        template <typename U> void fill(const U& v) {
            static_assert(Detail::is_strictly_assignable_v<T, U>, "Cannot assign fill value to entry type");
            for (auto& ele : data_) {
                if constexpr (sizeof...(Sizes) == 0)
                    ele = v;
                else
                    ele.fill(v);
            }
        }

        constexpr void swap(MultiArray<T, Size, Sizes...>& other) noexcept {
            data_.swap(other.data_);
        }
};

struct Console {
#if defined(_WIN32)
    HANDLE hConsole;
    bool vtEnabled;

    Console() {
        hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
        vtEnabled = false;

        if (hConsole == INVALID_HANDLE_VALUE) return;

        DWORD dwMode = 0;
        if (GetConsoleMode(hConsole, &dwMode)) {
            // Try enabling ANSI escape sequences
            dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
            if (SetConsoleMode(hConsole, dwMode)) {
                vtEnabled = true;
            }
        }
    }

    void setColor(int fg) {
        if (!vtEnabled) {
            // Windows Console API fallback
            SetConsoleTextAttribute(hConsole, fg);
        }
    }

#else
    // On Linux/macOS no init needed
    Console() {}
    void setColor(int) {}
#endif
};

struct COLORS {
    // ANSI codes for colors https://raw.githubusercontent.com/fidian/ansi/master/images/color-codes.png
    static constexpr std::string_view RESET = "\033[0m";

    // Basic colors
    static constexpr std::string_view BLACK = "\033[30m";
    static constexpr std::string_view RED = "\033[31m";
    static constexpr std::string_view GREEN = "\033[32m";
    static constexpr std::string_view YELLOW = "\033[33m";
    static constexpr std::string_view BLUE = "\033[34m";
    static constexpr std::string_view MAGENTA = "\033[35m";
    static constexpr std::string_view CYAN = "\033[36m";
    static constexpr std::string_view WHITE = "\033[37m";

    // Bright colors
    static constexpr std::string_view BRIGHT_BLACK = "\033[90m";
    static constexpr std::string_view BRIGHT_RED = "\033[91m";
    static constexpr std::string_view BRIGHT_GREEN = "\033[92m";
    static constexpr std::string_view BRIGHT_YELLOW = "\033[93m";
    static constexpr std::string_view BRIGHT_BLUE = "\033[94m";
    static constexpr std::string_view BRIGHT_MAGENTA = "\033[95m";
    static constexpr std::string_view BRIGHT_GYAN = "\033[96m";
    static constexpr std::string_view BRIGHT_WHITE = "\033[97m";

    static constexpr std::string_view GREY = BRIGHT_BLACK;
};

namespace CURSOR {
    static void clearAll(std::ostream& out = std::cout) { out << "\033[2J\033[H"; }
    static void clear(std::ostream& out = std::cout) { out << "\033[2K\r"; }
    static void clearDown(std::ostream& out = std::cout) { out << "\x1b[J"; }
    static void home(std::ostream& out = std::cout) { out << "\033[H"; }
    static void up(std::ostream& out = std::cout) { out << "\033[A"; }
    static void down(std::ostream& out = std::cout) { out << "\033[B"; }
    static void begin(std::ostream& out = std::cout) { out << "\033[1G"; }
    static void goTo(const size_t x, const size_t y, std::ostream& out = std::cout) { out << "\033[" << y << ";" << x << "H"; }

    static void hide(std::ostream& out = std::cout) { out << "\033[?25l"; }
    static void show(std::ostream& out = std::cout) { out << "\033[?25h"; }
}


inline void heatColor(float t, const std::string& text) {
    t = std::clamp(t, 0.0f, 1.0f);
    int r, g, b = 0;
    if (t < 0.5f) {
        const float ratio = t / 0.5f;
        r                 = 255;
        g                 = static_cast<int>(ratio * 255);
    }
    else {
        const float ratio = (t - 0.5f) / 0.5f;
        r                 = static_cast<int>(255 * (1.0f - ratio));
        g                 = 255;
    }

    fmt::print(fg(fmt::rgb(r, g, b)), "{}", text);
}

inline void heatColor(float t, const double v) {
    t = std::clamp(t, 0.0f, 1.0f);
    int r, g, b = 0;
    if (t < 0.5f) {
        const float ratio = t / 0.5f;
        r                 = 255;
        g                 = static_cast<int>(ratio * 255);
    }
    else {
        const float ratio = (t - 0.5f) / 0.5f;
        r                 = static_cast<int>(255 * (1.0f - ratio));
        g                 = 255;
    }

    fmt::print(fg(fmt::rgb(r, g, b)), "{:.2f}", v);
}

struct RGB { double r,g,b; };

static inline double sTolinear(double u){ 
    return (u <= 0.04045) ? (u/12.92) : std::pow((u+0.055)/1.055, 2.4); 
}
static inline double linearToS(double u){ 
    return (u <= 0.0031308) ? (12.92*u) : (1.055*std::pow(u, 1.0/2.4)-0.055); 
}

static inline RGB wdlRGB(int w, int d, int l){
    double W=w/1000.0, D=d/1000.0, L=l/1000.0;

    // anchors in sRGB 0..1
    RGB G{46/255.0, 204/255.0, 113/255.0};
    RGB N{128/255.0,128/255.0,128/255.0};
    RGB R{231/255.0, 76/255.0,  60/255.0};

    // convert to linear for perceptual blend
    RGB Gl{sTolinear(G.r), sTolinear(G.g), sTolinear(G.b)};
    RGB Nl{sTolinear(N.r), sTolinear(N.g), sTolinear(N.b)};
    RGB Rl{sTolinear(R.r), sTolinear(R.g), sTolinear(R.b)};

    RGB mix{
        W*Gl.r + D*Nl.r + L*Rl.r,
        W*Gl.g + D*Nl.g + L*Rl.g,
        W*Gl.b + D*Nl.b + L*Rl.b
    };

    // back to sRGB
    int R8 = (int)std::round(std::clamp(linearToS(mix.r),0.0,1.0)*255.0);
    int G8 = (int)std::round(std::clamp(linearToS(mix.g),0.0,1.0)*255.0);
    int B8 = (int)std::round(std::clamp(linearToS(mix.b),0.0,1.0)*255.0);

    return RGB{double(R8), double(G8), double(B8)}; // 0xRRGGBB
}

inline RGB rgbLerp(RGB a, RGB b, double t) {
    return { a.r + (b.r-a.r)*t, a.g + (b.g-a.g)*t, a.b + (b.b-a.b)*t };
}
static RGB scoreToRGB(double cp, double scale=1.0) {
    // clamp cp to [-scale, +scale]
    if (cp > scale) cp = scale;
    if (cp < -scale) cp = -scale;

    if (cp >= 0.0) {
        double t = cp / scale;
        return rgbLerp({240,240,240}, {76,175,80}, t); // gray → green
    } else {
        double t = -cp / scale;
        return rgbLerp({240,240,240}, {244,67,54}, t); // gray → red
    }
}

static int getTerminalWidth() {
#ifdef _WIN32
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    if (GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi))
        return csbi.srWindow.Right - csbi.srWindow.Left + 1;
    else
        return 80; // fallback
#else
    struct winsize w;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) == 0)
        return w.ws_col;
    else
        return 80; // fallback
#endif
}

// ---------------------- Pretty Stuff

static void printBoard(Board& board) {
    std::cout << COLORS::WHITE
            << "\u250c\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2510\n";

    int line = 1;
    for (int rank = 0; rank < 8; rank++) {
        std::cout << COLORS::WHITE << "\u2502 ";
        for (int file = 0; file < 8; file++) {
            Square sq = Square(Rank(rank), File(file));
            Piece p = board.at(sq);
            auto c = p.color() == Color::WHITE ? COLORS::YELLOW : COLORS::CYAN;
            if (p == Piece::NONE)
                c = COLORS::WHITE;
            std::cout << c << std::string(p) << COLORS::RESET << " ";
        }
        std::cout << COLORS::WHITE << "\u2502 \n";
    }
    std::cout << COLORS::WHITE
            << "\u2514\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2518\n";

}

static void printBoard(Board& board, Move move) {
    std::cout << COLORS::WHITE
            << "\u250c\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2510\n";

    int line = 1;
    Piece p;
    std::string_view c;
    auto doPieceColor = [&](Square sq) {
        p = board.at(sq);
        c = p.color() == Color::WHITE ? COLORS::YELLOW : COLORS::CYAN;
        if (p == Piece::NONE)
            c = COLORS::WHITE;
        if (sq == move.from())
            c = COLORS::GREY;
        if (sq == move.to()){
            c = COLORS::GREEN;
            p = board.at(move.from());
            if (move.typeOf() == Move::PROMOTION)
                p = Piece(move.promotionType(), board.sideToMove());
        }
    };
    for (int rank = 0; rank < 8; rank++) {
        std::cout << COLORS::WHITE << "\u2502 ";
        for (int file = 0; file < 8; file++) {
            Square sq = Square(Rank(7 - rank), File(file));
            doPieceColor(sq);
            std::cout << c << std::string(p) << COLORS::RESET << " ";
        }
        std::cout << COLORS::WHITE << "\u2502 " << (rank + 1) << "\n";
    }
    std::cout << COLORS::WHITE
            << "\u2514\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2518\n";
    std::cout << "  a b c d e f g h\n\n";
}

static void printBoard(Board& board, std::vector<Move> moves) {

    for (auto move : moves)
        std::cout << COLORS::WHITE << "\u250c\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2510  ";
    std::cout << COLORS::WHITE << "\u250c\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2510  ";
    std::cout << "\n";
    int line = 1;
    Piece p;
    std::string_view c;
    auto doPieceColor = [&](Board& testBoard, Move move, Square sq) {
        p = testBoard.at(sq);
        c = p.color() == Color::WHITE ? COLORS::YELLOW : COLORS::CYAN;
        if (p == Piece::NONE)
            c = COLORS::WHITE;
        if (move != Move::NO_MOVE){
            if (sq == move.from())
                c = COLORS::GREY;
            if (sq == move.to()){
                c = COLORS::GREEN;
                p = testBoard.at(move.from());
                if (move.typeOf() == Move::PROMOTION)
                    p = Piece(move.promotionType(), testBoard.sideToMove());
            }
        }
    };
    for (int rank = 0; rank < 8; rank++) {
        Board testBoard = board;

        std::cout << COLORS::WHITE << "\u2502 ";
        for (int file = 0; file < 8; file++) {
            Square sq = Square(Rank(7 - rank), File(file));
            doPieceColor(board, Move::NO_MOVE, sq);
            std::cout << c << std::string(p) << COLORS::RESET << " ";
        }
        std::cout << COLORS::WHITE << "\u2502 " << (rank + 1);

        for (Move move : moves) {
            std::cout << COLORS::WHITE << "\u2502 ";
            for (int file = 0; file < 8; file++) {
                Square sq = Square(Rank(7 - rank), File(file));
                doPieceColor(testBoard, move, sq);
                std::cout << c << std::string(p) << COLORS::RESET << " ";
            }
            std::cout << COLORS::WHITE << "\u2502 " << (rank + 1);
            testBoard.makeMove(move);
        }
        std::cout << "\n";
    }
    for (auto move : moves) {
        std::cout << COLORS::WHITE
                << "\u2514\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2518  ";
    }
    std::cout << COLORS::WHITE
                << "\u2514\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2518  ";
    std::cout << "\n";
    for (auto move : moves) {
        std::cout << "  a b c d e f g h    ";
    }
    std::cout << "  a b c d e f g h    ";
    std::cout << "\n\n";
    
}
