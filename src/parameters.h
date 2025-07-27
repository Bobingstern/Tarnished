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

//#define TUNE
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
constexpr int PAWN_HIST_ENTRIES = 1024;
constexpr int MAX_CORR_HIST = 1024;
// NNUE Parameters
constexpr int16_t HL_N = 1024;
constexpr int16_t QA = 255;
constexpr int16_t QB = 64;
constexpr int16_t NNUE_SCALE = 400;
constexpr int OUTPUT_BUCKETS = 8;

// Factorized LMR arrays
// {isQuiet, !isPV, improving, cutnode, ttpv, tthit, failhigh > 2}
const int LMR_ONE_COUNT = 7;
const int LMR_TWO_COUNT = LMR_ONE_COUNT * (LMR_ONE_COUNT - 1) / 2;
const int LMR_THREE_COUNT = LMR_ONE_COUNT * (LMR_ONE_COUNT - 1) * (LMR_ONE_COUNT - 2) / 6;
extern std::array<int, LMR_ONE_COUNT> LMR_ONE_PAIR;
extern std::array<int, LMR_TWO_COUNT> LMR_TWO_PAIR;
extern std::array<int, LMR_THREE_COUNT> LMR_THREE_PAIR;


std::list<TunableParam>& tunables();
TunableParam& addTunableParam(std::string name, int value, int min, int max, int step);
int lmrConvolution(std::array<bool, LMR_ONE_COUNT> features);
void printOBConfig();
void printWeatherFactoryConfig();

#define TUNABLE_PARAM(name, val, min, max, step)                                                                       \
    inline TunableParam& name##Param = addTunableParam(#name, val, min, max, step);                                    \
    inline int name() {                                                                                                \
        return name##Param.value;                                                                                      \
    }

// History Parameters
TUNABLE_PARAM(PAWN_CORR_WEIGHT, 210, 64, 2048, 32)
TUNABLE_PARAM(MAJOR_CORR_WEIGHT, 141, 64, 2048, 32)
TUNABLE_PARAM(MINOR_CORR_WEIGHT, 150, 64, 2048, 32)
TUNABLE_PARAM(NON_PAWN_STM_CORR_WEIGHT, 159, 64, 2048, 32)
TUNABLE_PARAM(NON_PAWN_NSTM_CORR_WEIGHT, 163, 64, 2048, 32)
TUNABLE_PARAM(CONT_CORR_WEIGHT, 147, 64, 2048, 32)
TUNABLE_PARAM(CORRHIST_BONUS_WEIGHT, 92, 10, 300, 10);

TUNABLE_PARAM(HIST_BONUS_QUADRATIC, 7, 1, 10, 1)
TUNABLE_PARAM(HIST_BONUS_LINEAR, 276, 64, 384, 32);
TUNABLE_PARAM(HIST_BONUS_OFFSET, 138, 64, 768, 64);

TUNABLE_PARAM(HIST_MALUS_QUADRATIC, 6, 1, 10, 1)
TUNABLE_PARAM(HIST_MALUS_LINEAR, 275, 64, 384, 32);
TUNABLE_PARAM(HIST_MALUS_OFFSET, 197, 64, 768, 64);

// Time Management
TUNABLE_PARAM(NODE_TM_BASE, 161, 30, 200, 5)
TUNABLE_PARAM(NODE_TM_SCALE, 141, 30, 200, 5)
TUNABLE_PARAM(COMPLEXITY_TM_SCALE, 82, 30, 200, 5)
TUNABLE_PARAM(COMPLEXITY_TM_BASE, 76, 30, 200, 5)
TUNABLE_PARAM(COMPLEXITY_TM_DIVISOR, 391, 200, 800, 10)

// SEE
TUNABLE_PARAM(PAWN_VALUE, 98, 50, 200, 7)
TUNABLE_PARAM(KNIGHT_VALUE, 324, 300, 700, 25)
TUNABLE_PARAM(BISHOP_VALUE, 323, 300, 700, 25)
TUNABLE_PARAM(ROOK_VALUE, 508, 400, 1000, 30)
TUNABLE_PARAM(QUEEN_VALUE, 999, 800, 1600, 40)

// Search Parameters
TUNABLE_PARAM(RFP_SCALE, 77, 30, 100, 8);
TUNABLE_PARAM(RFP_CORRPLEXITY_SCALE, 60, 16, 128, 5);

TUNABLE_PARAM(RAZORING_SCALE, 305, 100, 350, 40);

TUNABLE_PARAM(NMP_BASE_REDUCTION, 5, 2, 5, 1);
TUNABLE_PARAM(NMP_REDUCTION_SCALE, 3, 3, 6, 1);
TUNABLE_PARAM(NMP_EVAL_SCALE, 213, 50, 300, 10);

TUNABLE_PARAM(HIST_PRUNING_SCALE, 2501, 512, 4096, 64)

TUNABLE_PARAM(FP_SCALE, 146, 30, 200, 8)
TUNABLE_PARAM(FP_OFFSET, 92, 60, 350, 16)
TUNABLE_PARAM(FP_HIST_DIVISOR, 309, 128, 512, 16)

TUNABLE_PARAM(BNFP_DEPTH_SCALE, 122, 30, 200, 8)
TUNABLE_PARAM(BNFP_MOVECOUNT_SCALE, 366, 150, 500, 12)
TUNABLE_PARAM(BNFP_CAPTURED_SCALE, 80, 30, 200, 8)

TUNABLE_PARAM(SE_BETA_SCALE, 30, 8, 64, 1);
TUNABLE_PARAM(SE_DOUBLE_MARGIN, 21, 0, 40, 2);

// LMR Table
TUNABLE_PARAM(LMR_BASE_QUIET, 141, -50, 200, 5);
TUNABLE_PARAM(LMR_DIVISOR_QUIET, 273, 150, 350, 5);
TUNABLE_PARAM(LMR_BASE_NOISY, 24, -50, 200, 5);
TUNABLE_PARAM(LMR_DIVISOR_NOISY, 333, 150, 350, 5);
// Reduction Constants
TUNABLE_PARAM(LMR_HIST_DIVISOR, 8922, 4096, 16385, 650);
TUNABLE_PARAM(LMR_BASE_SCALE, 1058, 256, 2048, 64)
// Deeper/Shallower
TUNABLE_PARAM(LMR_DEEPER_BASE, 36, 16, 64, 4)
TUNABLE_PARAM(LMR_DEEPER_SCALE, 3, 3, 12, 1)

TUNABLE_PARAM(SEE_NOISY_SCALE, -86, -128, -16, 16)
TUNABLE_PARAM(SEE_QUIET_SCALE, -74, -128, -16, 16)
TUNABLE_PARAM(QS_SEE_MARGIN, 8, -2000, 200, 30)

TUNABLE_PARAM(MIN_ASP_WINDOW_DEPTH, 3, 3, 8, 1);
TUNABLE_PARAM(INITIAL_ASP_WINDOW, 31, 8, 64, 4);
TUNABLE_PARAM(ASP_WIDENING_FACTOR, 4, 1, 32, 2);