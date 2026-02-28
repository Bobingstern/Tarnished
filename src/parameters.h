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
#include "simd.h"

const int MAX_PLY = 125;
const int STACK_OVERHEAD = 6;
#define BENCH_DEPTH 13

// Random big num
#ifdef _MSC_VER
    #include <__msvc_int128.hpp>
using u128 = std::_Unsigned128;
#else
using u128 = unsigned __int128;
#endif

#define TUNE
// #define LMR_TUNE

const int TWO_WAY_COUNT = 11;
const int TWO_WAY_WEIGHT_COUNT = TWO_WAY_COUNT + TWO_WAY_COUNT * (TWO_WAY_COUNT - 1) / 2;
// Struct for tunable parameters
struct TunableParam {
    std::string name;
    int* value;
    int defaultValue;
    int min;
    int max;
    int step;
};


// // {isPV, ttPV, improving, oppWorsening, ttHit, cutnode, inCheck, ttMove, ttBound = EXACT, ttBound = FAIL_LOW, ttBound = BETA_CUT}
struct TwoWayParam {
    // flattened one way and two interactions
    std::array<int, TWO_WAY_WEIGHT_COUNT> weights = {0};
    std::array<int, TWO_WAY_WEIGHT_COUNT> defaults = {0};
    std::array<int, 1 << TWO_WAY_COUNT> lookup;
    int min;
    int max;
    int step;

    TwoWayParam() = default;

    TwoWayParam(const std::array<int, TWO_WAY_WEIGHT_COUNT>& def,
                int min_ = -256, int max_ = 256, int step_ = 16)
        : weights(def), defaults(def), min(min_), max(max_), step(step_)
    {}

    inline int operator()(const uint16_t& f) const {
        return lookup[f];
    }
    int twoWayConvolve(std::array<bool, TWO_WAY_COUNT>& features) {
        int output = 0;
        int twoIndex = 0;
        for (int i = 0; i < TWO_WAY_COUNT; i++) {
            output += weights[i] * features[i];
            for (int j = i + 1; j < TWO_WAY_COUNT; j++) {
                output += weights[TWO_WAY_COUNT + twoIndex] * (features[i] && features[j]);
                twoIndex++;
            }
        }
        return output;
    }
    void rebuildLookup() {
        for (uint32_t i = 0; i < lookup.size(); i++) {
            // Compute convolution
            std::array<bool, TWO_WAY_COUNT> f = {0};
            for (int j = 0; j < TWO_WAY_COUNT; j++) {
                f[j] = i & (1 << j);
            }
            lookup[i] = twoWayConvolve(f);
        }
    }
};


// History Constants
constexpr int16_t MAX_HISTORY = 16384;
constexpr int16_t MAX_HISTORY_BONUS = 4096;
const int16_t DEFAULT_HISTORY = 0;
constexpr int CORR_HIST_ENTRIES = 16384;
constexpr int PAWN_HIST_ENTRIES = 1024;
constexpr int MAX_CORR_HIST = 1024;

// NNUE Parameters
constexpr int16_t QA = 255;
constexpr int16_t QB = 64;
constexpr int16_t NNUE_SCALE = 369;
constexpr int OUTPUT_BUCKETS = 8;
constexpr int INPUT_BUCKETS = 16;
const bool HORIZONTAL_MIRROR = true;

constexpr int L1_SIZE = 1792;
constexpr int L2_SIZE = 16;
constexpr int L3_SIZE = 32;

constexpr int FT_SHIFT = 10;
constexpr float L1_MUL = float(1 << FT_SHIFT) / float(QA * QA * QB);
constexpr float WEIGHT_CLIPPING = 1.98f;

#ifndef AUTOVEC
constexpr int FT_CHUNK_SIZE = sizeof(vepi16) / sizeof(int16_t);
constexpr int L1_CHUNK_SIZE = sizeof(vepi8) / sizeof(int8_t);
constexpr int L2_CHUNK_SIZE = sizeof(vps32) / sizeof(float);
constexpr int L3_CHUNK_SIZE = sizeof(vps32) / sizeof(float);
constexpr int L1_CHUNK_PER_32 = sizeof(int32_t) / sizeof(int8_t);
#else
constexpr int L1_CHUNK_PER_32 = 1;
#endif

const std::array<int, 64> BUCKET_LAYOUT = {
  0,  1,  2,  3,  3,  2,  1,  0,
  4,  5,  6,  7,  7,  6,  5,  4,
  8,  9,  10, 11, 11, 10, 9,  8,
  8,  9,  10, 11, 11, 10, 9,  8,
  12, 12, 13, 13, 13, 13, 12, 12,
  12, 12, 13, 13, 13, 13, 12, 12,
  14, 14, 15, 15, 15, 15, 14, 14,
  14, 14, 15, 15, 15, 15, 14, 14,
};

// Factorized LMR arrays
// {isQuiet, !isPV, improving, cutnode, ttpv, tthit, failhigh > 2, corr > x}
const int LMR_ONE_COUNT = 8;
const int LMR_TWO_COUNT = LMR_ONE_COUNT * (LMR_ONE_COUNT - 1) / 2;
const int LMR_THREE_COUNT = LMR_ONE_COUNT * (LMR_ONE_COUNT - 1) * (LMR_ONE_COUNT - 2) / 6;
const int TOTAL_LMR_FEATURES = 1 << LMR_ONE_COUNT;

// LMR
inline std::array<int, LMR_ONE_COUNT> LMR_ONE_PAIR = {128, 1019, -880, 1835, -945, 211, 477, -522};
inline std::array<int, LMR_TWO_COUNT> LMR_TWO_PAIR = {
                                235, -227, 433, -177, -18, -1, 16, -529,
                                483, -47, 267, -159, -188, 155, -39, 234,
                                67, 123, 173, 125, 589, -628, 672, -32,
                                58, -193, -69, 60
                            };
inline std::array<int, LMR_THREE_COUNT> LMR_THREE_PAIR = {
                                -80, -556, 90, -69, -325, -75, 385, -399,
                                -117, -208, -271, -164, 516, 70, 115, 47,
                                -224, 244, -423, -152, -245, -120, 522, -99,
                                271, 123, 106, -194, -720, -12, 121, -286,
                                -160, 152, -176, 297, -314, -237, -220, -68,
                                -638, 127, 393, 263, 120, 229, 193, -151,
                                -531, -55, -69, -44, 365, -267, 254, -34
                            };

extern std::array<int, 6> MVV_VALUES;

const std::array<int, LMR_ONE_COUNT> LMR_ONE_PAIR_DEFAULT = LMR_ONE_PAIR;
const std::array<int, LMR_TWO_COUNT> LMR_TWO_PAIR_DEFAULT = LMR_TWO_PAIR;
const std::array<int, LMR_THREE_COUNT> LMR_THREE_PAIR_DEFAULT = LMR_THREE_PAIR;

extern bool PRETTY_PRINT;

int lmrConvolution(std::array<bool, LMR_ONE_COUNT> features);
void printOBConfig();
void printWeatherFactoryConfig();


inline std::list<TunableParam>& tunables() {
    static std::list<TunableParam> params;
    return params;
}

inline std::vector<TwoWayParam*>& twoWayParams()
{
    static std::vector<TwoWayParam*> list;
    return list;
}

inline TunableParam& addTunableParam(
    std::string name,
    int& value,
    int min,
    int max,
    int step)
{
    tunables().push_back({
        name,
        &value,
        value,
        min,
        max,
        step
    });

    return tunables().back();
}


inline void registerTwoWay(
    const std::string& baseName,
    TwoWayParam& param)
{
    for (int i = 0; i < TWO_WAY_WEIGHT_COUNT; ++i)
    {
        auto& t = addTunableParam(
            baseName + "[" + std::to_string(i) + "]",
            param.weights[i],
            param.min,
            param.max,
            param.step
        );

        t.defaultValue = param.defaults[i];
    }
    twoWayParams().push_back(&param);
    param.rebuildLookup();
}

inline TwoWayParam makeTwoWay(
    const std::array<int, TWO_WAY_WEIGHT_COUNT>& defaults,
    int min = -256, int max = 256, int step = 16)
{
    return TwoWayParam(defaults, min, max, step);
}


#define TUNABLE_PARAM(name, val, min, max, step) \
    inline int name##_value = val; \
    inline TunableParam& name##Param = \
        addTunableParam(#name, name##_value, min, max, step); \
    inline int name() { return name##_value; }

#define TUNABLE_TWOWAY(name, ...)                     \
    inline TwoWayParam& name() {                     \
        static TwoWayParam p(__VA_ARGS__);          \
        static bool registered =                     \
            (registerTwoWay(#name, p), true);       \
        return p;                                   \
    }

TUNABLE_PARAM(PAWN_CORR_WEIGHT, 187, 64, 2048, 32)
TUNABLE_PARAM(MAJOR_CORR_WEIGHT, 197, 64, 2048, 32)
TUNABLE_PARAM(MINOR_CORR_WEIGHT, 171, 64, 2048, 32)
TUNABLE_PARAM(NON_PAWN_STM_CORR_WEIGHT, 202, 64, 2048, 32)
TUNABLE_PARAM(NON_PAWN_NSTM_CORR_WEIGHT, 202, 64, 2048, 32)
TUNABLE_PARAM(CONT_CORR_WEIGHT, 265, 64, 2048, 32)
TUNABLE_PARAM(CORRHIST_BONUS_WEIGHT, 122, 10, 300, 10)
TUNABLE_PARAM(HIST_BONUS_QUADRATIC, 9, 1, 10, 1)
TUNABLE_PARAM(HIST_BONUS_LINEAR, 263, 64, 384, 16)
TUNABLE_PARAM(HIST_BONUS_OFFSET, -157, -768, -64, 16)
TUNABLE_PARAM(CONT_HIST_BONUS_QUADRATIC, 7, 1, 10, 1)
TUNABLE_PARAM(CONT_HIST_BONUS_LINEAR, 227, 64, 384, 16)
TUNABLE_PARAM(CONT_HIST_BONUS_OFFSET, -129, -768, -64, 16)
TUNABLE_PARAM(CAPT_HIST_BONUS_QUADRATIC, 5, 1, 10, 1)
TUNABLE_PARAM(CAPT_HIST_BONUS_LINEAR, 242, 64, 384, 16)
TUNABLE_PARAM(CAPT_HIST_BONUS_OFFSET, -184, -768, -64, 16)
TUNABLE_PARAM(HIST_MALUS_QUADRATIC, 4, 1, 10, 1)
TUNABLE_PARAM(HIST_MALUS_LINEAR, 263, 64, 384, 16)
TUNABLE_PARAM(HIST_MALUS_OFFSET, 79, 64, 768, 16)
TUNABLE_PARAM(CONT_HIST_MALUS_QUADRATIC, 8, 1, 10, 1)
TUNABLE_PARAM(CONT_HIST_MALUS_LINEAR, 293, 64, 384, 16)
TUNABLE_PARAM(CONT_HIST_MALUS_OFFSET, 248, 64, 768, 16)
TUNABLE_PARAM(CAPT_HIST_MALUS_QUADRATIC, 7, 1, 10, 1)
TUNABLE_PARAM(CAPT_HIST_MALUS_LINEAR, 303, 64, 384, 16)
TUNABLE_PARAM(CAPT_HIST_MALUS_OFFSET, 141, 64, 768, 16)
TUNABLE_PARAM(HIST_BONUS_MARGIN, 64, 16, 128, 6)

// Time Management
TUNABLE_PARAM(NODE_TM_BASE, 154, 30, 200, 5)
TUNABLE_PARAM(NODE_TM_SCALE, 152, 30, 200, 5)
TUNABLE_PARAM(COMPLEXITY_TM_SCALE, 87, 30, 200, 5)
TUNABLE_PARAM(COMPLEXITY_TM_BASE, 78, 30, 200, 5)
TUNABLE_PARAM(COMPLEXITY_TM_DIVISOR, 382, 200, 800, 10)
TUNABLE_PARAM(SOFT_TM_SCALE, 62, 2, 100, 2)
TUNABLE_PARAM(BM_STABILITY_BASE, 180, 30, 256, 5)
TUNABLE_PARAM(BM_STABILITY_SCALE, 10, 2, 100, 2)

// SEE
TUNABLE_PARAM(PAWN_VALUE, 105, 50, 200, 7)
TUNABLE_PARAM(KNIGHT_VALUE, 312, 300, 700, 25)
TUNABLE_PARAM(BISHOP_VALUE, 309, 300, 700, 25)
TUNABLE_PARAM(ROOK_VALUE, 492, 400, 1000, 30)
TUNABLE_PARAM(QUEEN_VALUE, 1003, 800, 1600, 40)

// SEE
TUNABLE_PARAM(MVV_PAWN_VALUE, 800, 256, 8192, 400)
TUNABLE_PARAM(MVV_KNIGHT_VALUE, 2400, 256, 8192, 400)
TUNABLE_PARAM(MVV_BISHOP_VALUE, 2400, 256, 8192, 400)
TUNABLE_PARAM(MVV_ROOK_VALUE, 4800, 256, 8192, 400)
TUNABLE_PARAM(MVV_QUEEN_VALUE, 7200, 256, 8192, 400)

// Threat ordering
TUNABLE_PARAM(THREAT_QUEEN_BONUS, 13224, 4092, 16384, 512)
TUNABLE_PARAM(THREAT_QUEEN_MALUS, 10828, 4092, 16384, 512)
TUNABLE_PARAM(THREAT_ROOK_BONUS, 10421, 4092, 16384, 512)
TUNABLE_PARAM(THREAT_ROOK_MALUS, 8149, 4092, 16384, 512)
TUNABLE_PARAM(THREAT_MINOR_BONUS, 7639, 4092, 16384, 512)
TUNABLE_PARAM(THREAT_MINOR_MALUS, 5967, 4092, 16384, 512)

// Material scaling
TUNABLE_PARAM(MAT_SCALE_PAWN, 105, 50, 200, 7)
TUNABLE_PARAM(MAT_SCALE_KNIGHT, 450, 300, 700, 25)
TUNABLE_PARAM(MAT_SCALE_BISHOP, 489, 300, 700, 25)
TUNABLE_PARAM(MAT_SCALE_ROOK, 678, 400, 1000, 30)
TUNABLE_PARAM(MAT_SCALE_QUEEN, 1299, 800, 1600, 40)
TUNABLE_PARAM(MAT_SCALE_BASE, 27349, 10000, 40000, 1500)

// Search Parameters
TUNABLE_PARAM(RFP_SCALE, 89, 30, 100, 8)
TUNABLE_PARAM(RFP_IMPROVING_SCALE, 70, 30, 100, 8)
TUNABLE_PARAM(RFP_CORRPLEXITY_SCALE, 37, 16, 128, 5)
TUNABLE_PARAM(RAZORING_SCALE, 321, 100, 350, 40)
TUNABLE_PARAM(NMP_BASE_REDUCTION, 5, 2, 5, 1)
TUNABLE_PARAM(NMP_REDUCTION_SCALE, 3, 3, 6, 1)
TUNABLE_PARAM(NMP_EVAL_SCALE, 195, 50, 300, 10)
TUNABLE_PARAM(NMP_BETA_M_OFFSET, 140, 50, 300, 10)
TUNABLE_PARAM(NMP_BETA_M_SCALE, 7, 2, 16, 1)
TUNABLE_PARAM(SPROBCUT_MARGIN, 400, 128, 750, 32)
TUNABLE_PARAM(HIST_PRUNING_SCALE, 2820, 512, 4096, 64)
TUNABLE_PARAM(FP_SCALE, 141, 30, 200, 8)
TUNABLE_PARAM(FP_OFFSET, 93, 60, 350, 16)
TUNABLE_PARAM(FP_HIST_DIVISOR, 93, 64, 512, 16)
TUNABLE_PARAM(BNFP_DEPTH_SCALE, 129, 30, 200, 8)
TUNABLE_PARAM(BNFP_MOVECOUNT_SCALE, 339, 150, 500, 12)
TUNABLE_PARAM(SE_BETA_SCALE, 27, 8, 64, 1)
TUNABLE_PARAM(SE_DOUBLE_MARGIN, 22, 0, 40, 2)
TUNABLE_PARAM(SE_TRIPLE_MARGIN, 91, 32, 128, 8)

// LMR Table
TUNABLE_PARAM(LMR_BASE_QUIET, 155, -50, 200, 5)
TUNABLE_PARAM(LMR_DIVISOR_QUIET, 257, 150, 350, 5)
TUNABLE_PARAM(LMR_BASE_NOISY, 26, -50, 200, 5)
TUNABLE_PARAM(LMR_DIVISOR_NOISY, 330, 150, 350, 5)

// Reduction Constants
TUNABLE_PARAM(LMR_QUIET_HIST_DIVISOR, 8212, 4096, 16385, 650)
TUNABLE_PARAM(LMR_NOISY_HIST_DIVISOR, 8212, 4096, 16385, 650)
TUNABLE_PARAM(LMR_BASE_SCALE, 1103, 256, 2048, 64)
TUNABLE_PARAM(LMR_CORR_MARGIN, 82, 32, 256, 9)

// Deeper/Shallower
TUNABLE_PARAM(LMR_DEEPER_BASE, 30, 16, 64, 4)
TUNABLE_PARAM(LMR_DEEPER_SCALE, 6, 3, 12, 1)
TUNABLE_PARAM(SEE_NOISY_SCALE, -90, -128, -16, 16)
TUNABLE_PARAM(SEE_QUIET_SCALE, -70, -128, -16, 16)
TUNABLE_PARAM(SEE_QUIET_HIST_DIVISOR, 99, 32, 256, 16)
TUNABLE_PARAM(SEE_NOISY_HIST_DIVISOR, 90, 32, 256, 16)
TUNABLE_PARAM(QS_SEE_MARGIN, -71, -2000, 200, 30)
TUNABLE_PARAM(MIN_ASP_WINDOW_DEPTH, 3, 3, 8, 1)
TUNABLE_PARAM(INITIAL_ASP_WINDOW, 28, 8, 64, 4)
TUNABLE_PARAM(ASP_WIDENING_FACTOR, 1, 1, 32, 2)

// Two way parameters
// Order is
// {isPV, ttPV, improving, oppWorsening, ttHit, cutnode, inCheck, ttMove, ttBound = EXACT, ttBound = FAIL_LOW, ttBound = BETA_CUT}
TUNABLE_TWOWAY(
    RFP_TWO_WAY,
    std::array<int, TWO_WAY_WEIGHT_COUNT>{0, 0, -70}
);
TUNABLE_TWOWAY(
    RAZORING_TWO_WAY,
    std::array<int, TWO_WAY_WEIGHT_COUNT>{0}
);
TUNABLE_TWOWAY(
    NMP_TWO_WAY,
    std::array<int, TWO_WAY_WEIGHT_COUNT>{0}
);
TUNABLE_TWOWAY(
    SMALL_PC_TWO_WAY,
    std::array<int, TWO_WAY_WEIGHT_COUNT>{0}
);
TUNABLE_TWOWAY(
    HP_TWO_WAY,
    std::array<int, TWO_WAY_WEIGHT_COUNT>{0}
);
TUNABLE_TWOWAY(
    FORWARD_FP_TWO_WAY,
    std::array<int, TWO_WAY_WEIGHT_COUNT>{0}
);
TUNABLE_TWOWAY(
    BNFP_TWO_WAY,
    std::array<int, TWO_WAY_WEIGHT_COUNT>{0}
);
TUNABLE_TWOWAY(
    SBETA_TWO_WAY,
    std::array<int, TWO_WAY_WEIGHT_COUNT>{0}
);

TUNABLE_TWOWAY(
    DEXT_TWO_WAY,
    std::array<int, TWO_WAY_WEIGHT_COUNT>{0}
);

TUNABLE_TWOWAY(
    TEXT_TWO_WAY,
    std::array<int, TWO_WAY_WEIGHT_COUNT>{0}
);

inline void initializeTwoWayParams() {
    RFP_TWO_WAY();
    RAZORING_TWO_WAY();
    NMP_TWO_WAY();
    SMALL_PC_TWO_WAY();
    HP_TWO_WAY();
    FORWARD_FP_TWO_WAY();
    BNFP_TWO_WAY();
    SBETA_TWO_WAY();
    DEXT_TWO_WAY();
    TEXT_TWO_WAY();
}