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

// #define TUNE
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
inline std::array<int, LMR_ONE_COUNT> LMR_ONE_PAIR = {-583, 1378, -1228, 1634, -1185, 131, 353, -573, };
inline std::array<int, LMR_TWO_COUNT> LMR_TWO_PAIR = {116, -45, 903, 500, 668, -455, -390, -105, 508, -887, 737, -420, -650, 542, -296, -107, 183, 550, -84, 742, 1166, -613, 456, 14, 137, -840, -419, 263, };
inline std::array<int, LMR_THREE_COUNT> LMR_THREE_PAIR = {-453, -396, 497, -413, 176, 31, 8, -673, -1076, -191, -81, -250, 171, -420, -266, 62, 328, 22, -405, -159, -842, 507, 1189, 525, 1258, 284, -312, -886, -1203, -157, 354, -487, -414, 622, -314, 267, 17, 392, -88, 451, -569, 169, 688, 322, -249, 305, 583, -207, -327, 487, -126, 172, 662, -410, 582, 1056, };

extern std::array<int, 6> MVV_VALUES;

const std::array<int, LMR_ONE_COUNT> LMR_ONE_PAIR_DEFAULT = LMR_ONE_PAIR;
const std::array<int, LMR_TWO_COUNT> LMR_TWO_PAIR_DEFAULT = LMR_TWO_PAIR;
const std::array<int, LMR_THREE_COUNT> LMR_THREE_PAIR_DEFAULT = LMR_THREE_PAIR;

extern bool PRETTY_PRINT;

int lmrConvolution(std::array<bool, LMR_ONE_COUNT> features);
void printOBConfig();
void printWeatherFactoryConfig();
void readSPSAOutput(const std::string& filename);

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

TUNABLE_PARAM(PAWN_CORR_WEIGHT, 251, 64, 2048, 32)
TUNABLE_PARAM(MAJOR_CORR_WEIGHT, 179, 64, 2048, 32)
TUNABLE_PARAM(MINOR_CORR_WEIGHT, 162, 64, 2048, 32)
TUNABLE_PARAM(NON_PAWN_STM_CORR_WEIGHT, 68, 64, 2048, 32)
TUNABLE_PARAM(NON_PAWN_NSTM_CORR_WEIGHT, 105, 64, 2048, 32)
TUNABLE_PARAM(CONT_CORR_WEIGHT, 286, 64, 2048, 32)
TUNABLE_PARAM(CORRHIST_BONUS_WEIGHT, 105, 10, 300, 10)
TUNABLE_PARAM(HIST_BONUS_QUADRATIC, 9, 1, 10, 1)
TUNABLE_PARAM(HIST_BONUS_LINEAR, 191, 64, 384, 16)
TUNABLE_PARAM(HIST_BONUS_OFFSET, -181, -768, -64, 16)
TUNABLE_PARAM(CONT_HIST_BONUS_QUADRATIC, 10, 1, 10, 1)
TUNABLE_PARAM(CONT_HIST_BONUS_LINEAR, 205, 64, 384, 16)
TUNABLE_PARAM(CONT_HIST_BONUS_OFFSET, -102, -768, -64, 16)
TUNABLE_PARAM(CAPT_HIST_BONUS_QUADRATIC, 6, 1, 10, 1)
TUNABLE_PARAM(CAPT_HIST_BONUS_LINEAR, 181, 64, 384, 16)
TUNABLE_PARAM(CAPT_HIST_BONUS_OFFSET, -185, -768, -64, 16)
TUNABLE_PARAM(HIST_MALUS_QUADRATIC, 3, 1, 10, 1)
TUNABLE_PARAM(HIST_MALUS_LINEAR, 235, 64, 384, 16)
TUNABLE_PARAM(HIST_MALUS_OFFSET, 75, 64, 768, 16)
TUNABLE_PARAM(CONT_HIST_MALUS_QUADRATIC, 10, 1, 10, 1)
TUNABLE_PARAM(CONT_HIST_MALUS_LINEAR, 280, 64, 384, 16)
TUNABLE_PARAM(CONT_HIST_MALUS_OFFSET, 223, 64, 768, 16)
TUNABLE_PARAM(CAPT_HIST_MALUS_QUADRATIC, 8, 1, 10, 1)
TUNABLE_PARAM(CAPT_HIST_MALUS_LINEAR, 262, 64, 384, 16)
TUNABLE_PARAM(CAPT_HIST_MALUS_OFFSET, 98, 64, 768, 16)
TUNABLE_PARAM(HIST_BONUS_MARGIN, 59, 16, 128, 6)
TUNABLE_PARAM(NODE_TM_BASE, 149, 30, 200, 5)
TUNABLE_PARAM(NODE_TM_SCALE, 147, 30, 200, 5)
TUNABLE_PARAM(COMPLEXITY_TM_SCALE, 82, 30, 200, 5)
TUNABLE_PARAM(COMPLEXITY_TM_BASE, 75, 30, 200, 5)
TUNABLE_PARAM(COMPLEXITY_TM_DIVISOR, 363, 200, 800, 10)
TUNABLE_PARAM(SOFT_TM_SCALE, 64, 2, 100, 2)
TUNABLE_PARAM(BM_STABILITY_BASE, 175, 30, 256, 5)
TUNABLE_PARAM(BM_STABILITY_SCALE, 15, 2, 100, 2)
TUNABLE_PARAM(PAWN_VALUE, 119, 50, 200, 7)
TUNABLE_PARAM(KNIGHT_VALUE, 351, 300, 700, 25)
TUNABLE_PARAM(BISHOP_VALUE, 382, 300, 700, 25)
TUNABLE_PARAM(ROOK_VALUE, 543, 400, 1000, 30)
TUNABLE_PARAM(QUEEN_VALUE, 1037, 800, 1600, 40)
TUNABLE_PARAM(MVV_PAWN_VALUE, 404, 256, 8192, 400)
TUNABLE_PARAM(MVV_KNIGHT_VALUE, 4392, 256, 8192, 400)
TUNABLE_PARAM(MVV_BISHOP_VALUE, 1274, 256, 8192, 400)
TUNABLE_PARAM(MVV_ROOK_VALUE, 4811, 256, 8192, 400)
TUNABLE_PARAM(MVV_QUEEN_VALUE, 6928, 256, 8192, 400)
TUNABLE_PARAM(THREAT_QUEEN_BONUS, 13368, 4092, 16384, 512)
TUNABLE_PARAM(THREAT_QUEEN_MALUS, 10668, 4092, 16384, 512)
TUNABLE_PARAM(THREAT_ROOK_BONUS, 9868, 4092, 16384, 512)
TUNABLE_PARAM(THREAT_ROOK_MALUS, 7713, 4092, 16384, 512)
TUNABLE_PARAM(THREAT_MINOR_BONUS, 6051, 4092, 16384, 512)
TUNABLE_PARAM(THREAT_MINOR_MALUS, 7476, 4092, 16384, 512)
TUNABLE_PARAM(MAT_SCALE_PAWN, 122, 50, 200, 7)
TUNABLE_PARAM(MAT_SCALE_KNIGHT, 388, 300, 700, 25)
TUNABLE_PARAM(MAT_SCALE_BISHOP, 529, 300, 700, 25)
TUNABLE_PARAM(MAT_SCALE_ROOK, 718, 400, 1000, 30)
TUNABLE_PARAM(MAT_SCALE_QUEEN, 1394, 800, 1600, 40)
TUNABLE_PARAM(MAT_SCALE_BASE, 27048, 10000, 40000, 1500)
TUNABLE_PARAM(RFP_SCALE, 98, 30, 100, 8)
TUNABLE_PARAM(RFP_IMPROVING_SCALE, 70, 30, 100, 8)
TUNABLE_PARAM(RFP_CORRPLEXITY_SCALE, 56, 16, 128, 5)
TUNABLE_PARAM(RAZORING_SCALE, 258, 100, 350, 40)
TUNABLE_PARAM(NMP_BASE_REDUCTION, 5, 2, 5, 1)
TUNABLE_PARAM(NMP_REDUCTION_SCALE, 3, 3, 6, 1)
TUNABLE_PARAM(NMP_EVAL_SCALE, 170, 50, 300, 10)
TUNABLE_PARAM(NMP_BETA_M_OFFSET, 180, 50, 300, 10)
TUNABLE_PARAM(NMP_BETA_M_SCALE, 8, 2, 16, 1)
TUNABLE_PARAM(SPROBCUT_MARGIN, 459, 128, 750, 32)
TUNABLE_PARAM(HIST_PRUNING_SCALE, 2597, 512, 4096, 64)
TUNABLE_PARAM(FP_SCALE, 132, 30, 200, 8)
TUNABLE_PARAM(FP_OFFSET, 95, 60, 350, 16)
TUNABLE_PARAM(FP_HIST_DIVISOR, 86, 64, 512, 16)
TUNABLE_PARAM(BNFP_DEPTH_SCALE, 133, 30, 200, 8)
TUNABLE_PARAM(BNFP_MOVECOUNT_SCALE, 362, 150, 500, 12)
TUNABLE_PARAM(SE_BETA_SCALE, 27, 8, 64, 1)
TUNABLE_PARAM(SE_DOUBLE_MARGIN, 30, 0, 40, 2)
TUNABLE_PARAM(SE_TRIPLE_MARGIN, 82, 32, 128, 8)
TUNABLE_PARAM(LMR_BASE_QUIET, 155, -50, 200, 5)
TUNABLE_PARAM(LMR_DIVISOR_QUIET, 264, 150, 350, 5)
TUNABLE_PARAM(LMR_BASE_NOISY, 16, -50, 200, 5)
TUNABLE_PARAM(LMR_DIVISOR_NOISY, 329, 150, 350, 5)
TUNABLE_PARAM(LMR_QUIET_HIST_DIVISOR, 9840, 4096, 16385, 650)
TUNABLE_PARAM(LMR_NOISY_HIST_DIVISOR, 7072, 4096, 16385, 650)
TUNABLE_PARAM(LMR_BASE_SCALE, 1105, 256, 2048, 64)
TUNABLE_PARAM(LMR_CORR_MARGIN, 75, 32, 256, 9)
TUNABLE_PARAM(LMR_DEEPER_BASE, 32, 16, 64, 4)
TUNABLE_PARAM(LMR_DEEPER_SCALE, 8, 3, 12, 1)
TUNABLE_PARAM(SEE_NOISY_SCALE, -99, -128, -16, 16)
TUNABLE_PARAM(SEE_QUIET_SCALE, -85, -128, -16, 16)
TUNABLE_PARAM(SEE_QUIET_HIST_DIVISOR, 95, 32, 256, 16)
TUNABLE_PARAM(SEE_NOISY_HIST_DIVISOR, 95, 32, 256, 16)
TUNABLE_PARAM(QS_SEE_MARGIN, -138, -2000, 200, 30)
TUNABLE_PARAM(MIN_ASP_WINDOW_DEPTH, 4, 3, 8, 1)
TUNABLE_PARAM(INITIAL_ASP_WINDOW, 27, 8, 64, 4)
TUNABLE_PARAM(ASP_WIDENING_FACTOR, 4, 1, 32, 2)

// Two way parameters
// Order is
// {isPV, ttPV, improving, oppWorsening, ttHit, cutnode, inCheck, ttMove, ttBound = EXACT, ttBound = FAIL_LOW, ttBound = BETA_CUT}
TUNABLE_TWOWAY(
    RFP_TWO_WAY,
    std::array<int, TWO_WAY_WEIGHT_COUNT>{-78, -60, -121, -43, 27, -11, 6, 36, -26, 57, -24, -4, 8, -17, 56, -19, -14, 26, -7, -71, -45, 3, 4, -20, 11, -14, -6, -3, 45, -1, -20, 17, -8, 43, 61, -26, -36, -3, -39, -28, 15, -36, 27, -75, 35, -40, -17, 31, 29, 4, 47, 12, 15, -90, -13, 13, -19, -17, -11, 0, 35, 60, -19, -21, -42, -8}
);
TUNABLE_TWOWAY(
    RAZORING_TWO_WAY,
    std::array<int, TWO_WAY_WEIGHT_COUNT>{30, 44, 32, -45, -8, 54, 49, -31, 58, 7, 34, -14, -25, 49, -34, 88, -25, 60, 3, -8, 1, -39, 23, 2, 21, -3, 19, 44, -61, 17, 10, -17, 2, 41, -73, -18, 7, -4, 37, 19, 64, -55, 36, 20, 44, 46, -18, -16, 10, 13, -27, -33, 31, -16, 5, -51, 3, -14, 33, -26, 14, -8, -22, -25, 2, 45}
);
TUNABLE_TWOWAY(
    NMP_TWO_WAY,
    std::array<int, TWO_WAY_WEIGHT_COUNT>{20, 20, -44, 10, -28, 16, 24, -21, -31, 3, 30, -3, 13, 24, -15, 33, -44, 73, 11, 26, 6, -25, -1, -13, -22, -15, -18, 15, -30, 51, 29, 41, -7, 8, -92, 40, 4, 26, -62, -43, 18, 32, -15, 7, 44, 1, 13, -51, 18, -14, 15, 23, -43, -34, -3, -46, -12, 7, 31, 3, -55, -7, -4, 49, -41, 17}
);
TUNABLE_TWOWAY(
    SMALL_PC_TWO_WAY,
    std::array<int, TWO_WAY_WEIGHT_COUNT>{-8, 35, 57, -6, 5, 35, 36, -19, -35, 21, -24, -69, 32, 40, -28, -86, 23, 24, -31, 50, -8, 62, 24, -38, 64, -31, 14, 46, 16, 15, 4, 49, 20, 46, 11, 33, 14, 10, -22, -24, -29, -29, 58, -2, 64, -18, 11, 7, 20, -47, -46, -40, -47, 19, -47, 31, 51, -77, -3, -15, -47, -36, 51, -49, -16, -17}
);
TUNABLE_TWOWAY(
    HP_TWO_WAY,
    std::array<int, TWO_WAY_WEIGHT_COUNT>{-16, -8, 16, 25, 23, 79, 72, 12, -10, 48, 19, -44, 7, 25, 83, -4, -28, 25, -1, -26, 46, 0, -26, 39, -42, 41, -30, -31, 9, 34, 55, 25, 24, -52, -34, 0, -20, -27, 45, 21, -35, 22, -58, -4, 28, 27, -36, 18, -6, -8, -27, 15, -16, -73, 17, 11, -11, -35, 24, -8, -34, 30, -29, 33, -29, -28}
);
TUNABLE_TWOWAY(
    FORWARD_FP_TWO_WAY,
    std::array<int, TWO_WAY_WEIGHT_COUNT>{19, -26, 5, -12, 33, 25, 11, -38, -14, -38, 9, -4, 3, -4, 34, -24, -40, 4, -60, -35, -13, 1, -51, 19, 43, -1, -10, -45, 5, 9, 9, 66, -49, -4, 36, 0, 5, -11, 36, 58, -5, 53, 24, 54, -30, 31, 15, 3, 16, 21, 57, 50, -1, 55, 43, 20, 18, -30, -17, 5, 53, -44, 24, 43, -29, -52}
);
TUNABLE_TWOWAY(
    BNFP_TWO_WAY,
    std::array<int, TWO_WAY_WEIGHT_COUNT>{18, 35, 13, 70, -30, 0, -6, -22, 1, 39, -18, 15, -22, -50, 2, 22, 20, -4, -63, -19, -5, 5, 17, 14, -37, 16, -15, 57, 35, 71, -16, -3, 57, 26, -14, 8, 34, 27, -6, -37, -23, 25, 10, 13, -6, 32, 33, 4, -38, 63, 8, -7, 39, -43, 21, 32, 15, -36, -1, 20, 42, -6, -43, 2, -9, 17}
);
TUNABLE_TWOWAY(
    SBETA_TWO_WAY,
    std::array<int, TWO_WAY_WEIGHT_COUNT>{14, 6, 5, 19, -14, 18, 57, -7, -4, 9, -21, -8, -9, -14, -5, 48, 33, 18, -42, 38, 62, -8, 36, -4, -10, 3, -25, -25, -7, -23, -10, 32, -3, 4, 27, 21, 27, 3, -26, 33, 10, -15, 12, -53, 21, 7, 33, 29, 11, 1, -17, -13, -20, -4, 3, -1, 30, 12, -26, -67, -27, -20, 15, 32, 24, -3}
);
TUNABLE_TWOWAY(
    DEXT_TWO_WAY,
    std::array<int, TWO_WAY_WEIGHT_COUNT>{61, 27, -20, 4, -21, 34, 27, 35, -39, 30, 0, 10, 0, -21, -22, 2, 22, 4, -18, 41, 50, 26, -21, 3, 71, 45, -4, -32, -11, 31, 42, 7, 27, -16, 35, -28, 0, 10, 84, 0, -21, -32, 37, -17, -12, 4, -19, 26, 1, -18, -49, 14, -15, -15, -29, -8, -16, -31, -84, -3, 19, -24, -1, 67, -43, -17}
);
TUNABLE_TWOWAY(
    TEXT_TWO_WAY,
    std::array<int, TWO_WAY_WEIGHT_COUNT>{3, 34, -15, 19, 20, -33, -21, -7, -25, 31, -45, -6, -9, -57, 12, -2, -62, -121, 29, -49, -90, 27, -6, 12, -17, 26, -47, 16, 33, -5, -27, 36, -17, -7, -54, 69, 48, -47, 29, 23, -10, -25, 8, -18, 6, -19, 25, 17, 74, 68, -14, 4, 36, 34, 3, 68, 50, -20, -1, 26, 20, -19, -84, -7, 6, 30}
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