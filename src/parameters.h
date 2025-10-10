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

const int MAX_PLY = 125;
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
constexpr int PAWN_HIST_ENTRIES = 1024;
constexpr int MAX_CORR_HIST = 1024;
// NNUE Parameters
constexpr int16_t HL_N = 1280;
constexpr int16_t QA = 255;
constexpr int16_t QB = 64;
constexpr int16_t NNUE_SCALE = 400;
constexpr int OUTPUT_BUCKETS = 8;
constexpr int INPUT_BUCKETS = 8;
const bool HORIZONTAL_MIRROR = true;

const std::array<int, 64> BUCKET_LAYOUT = {
    0, 1, 2, 3, 3, 2, 1, 0,
    4, 4, 5, 5, 5, 5, 4, 4,
    6, 6, 6, 6, 6, 6, 6, 6,
    6, 6, 6, 6, 6, 6, 6, 6,
    6, 6, 6, 6, 6, 6, 6, 6,
    7, 7, 7, 7, 7, 7, 7, 7,
    7, 7, 7, 7, 7, 7, 7, 7,
    7, 7, 7, 7, 7, 7, 7, 7,
};

// Factorized LMR arrays
// {isQuiet, !isPV, improving, cutnode, ttpv, tthit, failhigh > 2, corr > x}
const int LMR_ONE_COUNT = 8;
const int LMR_TWO_COUNT = LMR_ONE_COUNT * (LMR_ONE_COUNT - 1) / 2;
const int LMR_THREE_COUNT = LMR_ONE_COUNT * (LMR_ONE_COUNT - 1) * (LMR_ONE_COUNT - 2) / 6;
extern std::array<int, LMR_ONE_COUNT> LMR_ONE_PAIR;
extern std::array<int, LMR_TWO_COUNT> LMR_TWO_PAIR;
extern std::array<int, LMR_THREE_COUNT> LMR_THREE_PAIR;

extern bool PRETTY_PRINT;

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
TUNABLE_PARAM(PAWN_CORR_WEIGHT, 215, 64, 2048, 32)
TUNABLE_PARAM(MAJOR_CORR_WEIGHT, 148, 64, 2048, 32)
TUNABLE_PARAM(MINOR_CORR_WEIGHT, 147, 64, 2048, 32)
TUNABLE_PARAM(NON_PAWN_STM_CORR_WEIGHT, 181, 64, 2048, 32)
TUNABLE_PARAM(NON_PAWN_NSTM_CORR_WEIGHT, 183, 64, 2048, 32)
TUNABLE_PARAM(CONT_CORR_WEIGHT, 196, 64, 2048, 32)
TUNABLE_PARAM(CORRHIST_BONUS_WEIGHT, 87, 10, 300, 10);

TUNABLE_PARAM(HIST_BONUS_QUADRATIC, 6, 1, 10, 1)
TUNABLE_PARAM(HIST_BONUS_LINEAR, 241, 64, 384, 32);
TUNABLE_PARAM(HIST_BONUS_OFFSET, 136, 64, 768, 64);

TUNABLE_PARAM(HIST_MALUS_QUADRATIC, 6, 1, 10, 1)
TUNABLE_PARAM(HIST_MALUS_LINEAR, 273, 64, 384, 32);
TUNABLE_PARAM(HIST_MALUS_OFFSET, 179, 64, 768, 64);

// Time Management
TUNABLE_PARAM(NODE_TM_BASE, 164, 30, 200, 5)
TUNABLE_PARAM(NODE_TM_SCALE, 145, 30, 200, 5)
TUNABLE_PARAM(COMPLEXITY_TM_SCALE, 83, 30, 200, 5)
TUNABLE_PARAM(COMPLEXITY_TM_BASE, 77, 30, 200, 5)
TUNABLE_PARAM(COMPLEXITY_TM_DIVISOR, 386, 200, 800, 10)

// SEE
TUNABLE_PARAM(PAWN_VALUE, 100, 50, 200, 7)
TUNABLE_PARAM(KNIGHT_VALUE, 311, 300, 700, 25)
TUNABLE_PARAM(BISHOP_VALUE, 334, 300, 700, 25)
TUNABLE_PARAM(ROOK_VALUE, 506, 400, 1000, 30)
TUNABLE_PARAM(QUEEN_VALUE, 984, 800, 1600, 40)

// Material scaling
TUNABLE_PARAM(MAT_SCALE_PAWN, 108, 50, 200, 7)
TUNABLE_PARAM(MAT_SCALE_KNIGHT, 450, 300, 700, 25)
TUNABLE_PARAM(MAT_SCALE_BISHOP, 459, 300, 700, 25)
TUNABLE_PARAM(MAT_SCALE_ROOK, 648, 400, 1000, 30)
TUNABLE_PARAM(MAT_SCALE_QUEEN, 1228, 800, 1600, 40)
TUNABLE_PARAM(MAT_SCALE_BASE, 26411, 10000, 40000, 1500)

// Search Parameters
TUNABLE_PARAM(RFP_SCALE, 82, 30, 100, 8);
TUNABLE_PARAM(RFP_IMPROVING_SCALE, 75, 30, 100, 8);
TUNABLE_PARAM(RFP_CORRPLEXITY_SCALE, 58, 16, 128, 5);

TUNABLE_PARAM(RAZORING_SCALE, 312, 100, 350, 40);

TUNABLE_PARAM(NMP_BASE_REDUCTION, 5, 2, 5, 1);
TUNABLE_PARAM(NMP_REDUCTION_SCALE, 3, 3, 6, 1);
TUNABLE_PARAM(NMP_EVAL_SCALE, 207, 50, 300, 10);

TUNABLE_PARAM(HIST_PRUNING_SCALE, 2536, 512, 4096, 64)

TUNABLE_PARAM(FP_SCALE, 141, 30, 200, 8)
TUNABLE_PARAM(FP_OFFSET, 106, 60, 350, 16)
TUNABLE_PARAM(FP_HIST_DIVISOR, 318, 128, 512, 16)

TUNABLE_PARAM(BNFP_DEPTH_SCALE, 122, 30, 200, 8)
TUNABLE_PARAM(BNFP_MOVECOUNT_SCALE, 364, 150, 500, 12)

TUNABLE_PARAM(SE_BETA_SCALE, 30, 8, 64, 1);
TUNABLE_PARAM(SE_DOUBLE_MARGIN, 22, 0, 40, 2);

// LMR Table
TUNABLE_PARAM(LMR_BASE_QUIET, 137, -50, 200, 5);
TUNABLE_PARAM(LMR_DIVISOR_QUIET, 275, 150, 350, 5);
TUNABLE_PARAM(LMR_BASE_NOISY, 24, -50, 200, 5);
TUNABLE_PARAM(LMR_DIVISOR_NOISY, 331, 150, 350, 5);
// Reduction Constants
TUNABLE_PARAM(LMR_HIST_DIVISOR, 8687, 4096, 16385, 650);
TUNABLE_PARAM(LMR_BASE_SCALE, 1015, 256, 2048, 64)
TUNABLE_PARAM(LMR_CORR_MARGIN, 71, 32, 256, 9);
// Deeper/Shallower
TUNABLE_PARAM(LMR_DEEPER_BASE, 33, 16, 64, 4)
TUNABLE_PARAM(LMR_DEEPER_SCALE, 3, 3, 12, 1)

TUNABLE_PARAM(SEE_NOISY_SCALE, -85, -128, -16, 16)
TUNABLE_PARAM(SEE_QUIET_SCALE, -73, -128, -16, 16)
TUNABLE_PARAM(QS_SEE_MARGIN, -42, -2000, 200, 30)

TUNABLE_PARAM(MIN_ASP_WINDOW_DEPTH, 4, 3, 8, 1);
TUNABLE_PARAM(INITIAL_ASP_WINDOW, 29, 8, 64, 4);
TUNABLE_PARAM(ASP_WIDENING_FACTOR, 4, 1, 32, 2);