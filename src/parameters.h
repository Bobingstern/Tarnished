#pragma once
#include <array>
#include <bit>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <list>
#include <sstream>
#include <vector>

#define MAX_PLY 125
#define BENCH_DEPTH 13

// Random big num
#ifdef _MSC_VER
    #include <__msvc_int128.hpp>
using u128 = std::_Unsigned128;
#else
using u128 = unsigned __int128;
#endif

// #define TUNE
// #define LMR_TUNE

// Struct for tunable parameters
struct TunableParam {
        std::string name;
        int value;
        int defaultValue;
        int min;
        int max;
        int step;
};

// History Constants
constexpr int16_t MAX_HISTORY = 16383;
const int16_t DEFAULT_HISTORY = 0;
constexpr int CORR_HIST_ENTRIES = 16384;
constexpr int MAX_CORR_HIST = 1024;
// NNUE Parameters
constexpr int16_t HL_N = 1024;
constexpr int16_t QA = 255;
constexpr int16_t QB = 64;
constexpr int16_t NNUE_SCALE = 400;
constexpr int OUTPUT_BUCKETS = 8;

// Factorized LMR arrays
// {isQuiet, !isPV, improving, cutnode, ttpv, tthit}
const int LMR_ONE_COUNT = 6;
const int LMR_TWO_COUNT = LMR_ONE_COUNT * (LMR_ONE_COUNT - 1) / 2;
const int LMR_THREE_COUNT = LMR_ONE_COUNT * (LMR_ONE_COUNT - 1) * (LMR_ONE_COUNT - 2) / 6;
extern std::array<int, LMR_ONE_COUNT> LMR_ONE_PAIR;
extern std::array<int, LMR_TWO_COUNT> LMR_TWO_PAIR;
extern std::array<int, LMR_THREE_COUNT> LMR_THREE_PAIR;


std::list<TunableParam>& tunables();
TunableParam& addTunableParam(std::string name, int value, int min, int max, int step);
int lmrConvolution(std::array<bool, LMR_ONE_COUNT> features);
void printWeatherFactoryConfig();

#define TUNABLE_PARAM(name, val, min, max, step)                                                                       \
    inline TunableParam& name##Param = addTunableParam(#name, val, min, max, step);                                    \
    inline int name() {                                                                                                \
        return name##Param.value;                                                                                      \
    }

// History Parameters
TUNABLE_PARAM(PAWN_CORR_WEIGHT, 186, 64, 2048, 32)
TUNABLE_PARAM(MAJOR_CORR_WEIGHT, 128, 64, 2048, 32)
TUNABLE_PARAM(MINOR_CORR_WEIGHT, 128, 64, 2048, 32)
TUNABLE_PARAM(NON_PAWN_STM_CORR_WEIGHT, 128, 64, 2048, 32)
TUNABLE_PARAM(NON_PAWN_NSTM_CORR_WEIGHT, 128, 64, 2048, 32)
TUNABLE_PARAM(CORRHIST_BONUS_WEIGHT, 100, 10, 300, 10);

TUNABLE_PARAM(HIST_BONUS_QUADRATIC, 7, 1, 10, 1)
TUNABLE_PARAM(HIST_BONUS_LINEAR, 274, 64, 384, 32);
TUNABLE_PARAM(HIST_BONUS_OFFSET, 182, 64, 768, 64);

TUNABLE_PARAM(HIST_MALUS_QUADRATIC, 5, 1, 10, 1)
TUNABLE_PARAM(HIST_MALUS_LINEAR, 283, 64, 384, 32);
TUNABLE_PARAM(HIST_MALUS_OFFSET, 169, 64, 768, 64);

// Search Parameters
TUNABLE_PARAM(RFP_SCALE, 76, 30, 100, 8);
TUNABLE_PARAM(RFP_CORRPLEXITY_SCALE, 64, 16, 128, 5);

TUNABLE_PARAM(RAZORING_SCALE, 300, 100, 350, 40);

TUNABLE_PARAM(NMP_BASE_REDUCTION, 4, 2, 5, 1);
TUNABLE_PARAM(NMP_REDUCTION_SCALE, 4, 3, 6, 1);
TUNABLE_PARAM(NMP_EVAL_SCALE, 210, 50, 300, 10);

TUNABLE_PARAM(HIST_PRUNING_SCALE, 2500, 512, 4096, 64)

TUNABLE_PARAM(SE_MIN_DEPTH, 7, 4, 10, 1);
TUNABLE_PARAM(SE_BETA_SCALE, 31, 8, 64, 1);
TUNABLE_PARAM(SE_DOUBLE_MARGIN, 22, 0, 40, 2);

// LMR Table
TUNABLE_PARAM(LMR_BASE_QUIET, 137, -50, 200, 5);
TUNABLE_PARAM(LMR_DIVISOR_QUIET, 275, 150, 350, 5);
TUNABLE_PARAM(LMR_BASE_NOISY, 17, -50, 200, 5);
TUNABLE_PARAM(LMR_DIVISOR_NOISY, 329, 150, 350, 5);
// Reduction Constants
TUNABLE_PARAM(LMR_HIST_DIVISOR, 7952, 4096, 16385, 650);
TUNABLE_PARAM(LMR_BASE_SCALE, 979, 256, 2048, 64)
// Deeper/Shallower
TUNABLE_PARAM(LMR_DEEPER_BASE, 38, 16, 64, 4)
TUNABLE_PARAM(LMR_DEEPER_SCALE, 4, 3, 12, 1)

TUNABLE_PARAM(IIR_MIN_DEPTH, 5, 2, 9, 1);

TUNABLE_PARAM(LMP_MIN_MOVES_BASE, 2, 2, 8, 1);
TUNABLE_PARAM(LMP_DEPTH_SCALE, 1, 1, 10, 1);

TUNABLE_PARAM(SEE_PRUNING_SCALAR, -90, -128, -16, 16)

TUNABLE_PARAM(MIN_ASP_WINDOW_DEPTH, 4, 3, 8, 1);
TUNABLE_PARAM(INITIAL_ASP_WINDOW, 37, 8, 64, 4);
TUNABLE_PARAM(ASP_WIDENING_FACTOR, 3, 1, 32, 2);