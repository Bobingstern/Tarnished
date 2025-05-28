#pragma once

#include <bit>
#include <vector>
#include <sstream>
#include <cassert>
#include <cstring>
#include <cmath>
#include <list>

#define MAX_PLY 125
#define BENCH_DEPTH 12

//#define TUNE

// Struct for tunable parameters
struct TunableParam
{
    std::string name;
    int value;
    int defaultValue;
    int min;
    int max;
    int step;
};

std::list<TunableParam>& tunables();
TunableParam& addTunableParam(std::string name, int value, int min, int max, int step);
void printWeatherFactoryConfig();

#define TUNABLE_PARAM(name, val, min, max, step) \
    inline TunableParam& name##Param = addTunableParam(#name, val, min, max, step); \
    inline int name() { return name##Param.value; }


// NNUE Parameters
constexpr int16_t HL_N = 512;
constexpr int16_t QA = 255;
constexpr int16_t QB = 64;
constexpr int16_t NNUE_SCALE = 400;
constexpr int OUTPUT_BUCKETS = 8; 

// History
constexpr int16_t MAX_HISTORY = 16383;
const int16_t DEFAULT_HISTORY = 0;
constexpr int PAWN_CORR_HIST_ENTRIES = 16384;
constexpr int MAX_CORR_HIST = 1024;

TUNABLE_PARAM(PAWN_CORR_WEIGHT, 128, 64, 2048, 32)
TUNABLE_PARAM(CORRHIST_BONUS_WEIGHT, 100, 10, 300, 10);

TUNABLE_PARAM(HIST_BONUS_QUADRATIC, 8, 1, 10, 1)
TUNABLE_PARAM(HIST_BONUS_LINEAR, 212, 64, 384, 32);
TUNABLE_PARAM(HIST_BONUS_OFFSET, 159, 64, 768, 64);

TUNABLE_PARAM(HIST_MALUS_QUADRATIC, 5, 1, 10, 1)
TUNABLE_PARAM(HIST_MALUS_LINEAR, 250, 64, 384, 32);
TUNABLE_PARAM(HIST_MALUS_OFFSET, 66, 64, 768, 64);

// Search Parameters
TUNABLE_PARAM(RFP_MARGIN, 70, 30, 100, 8);
TUNABLE_PARAM(RFP_MAX_DEPTH, 6, 4, 10, 1);

TUNABLE_PARAM(NMP_BASE_REDUCTION, 3, 2, 5, 1);
TUNABLE_PARAM(NMP_REDUCTION_SCALE, 4, 3, 6, 1);
TUNABLE_PARAM(NMP_EVAL_SCALE, 200, 50, 300, 10);

TUNABLE_PARAM(SE_MIN_DEPTH, 8, 4, 10, 1);
TUNABLE_PARAM(SE_BETA_SCALE, 32, 8, 64, 1);
TUNABLE_PARAM(SE_DOUBLE_MARGIN, 20, 0, 40, 2);

TUNABLE_PARAM(LMR_BASE_QUIET, 135, -50, 200, 5);
TUNABLE_PARAM(LMR_DIVISOR_QUIET, 275, 150, 350, 5);
TUNABLE_PARAM(LMR_BASE_NOISY, 20, -50, 200, 5);
TUNABLE_PARAM(LMR_DIVISOR_NOISY, 335, 150, 350, 5);
TUNABLE_PARAM(LMR_MIN_DEPTH, 2, 1, 8, 1);
TUNABLE_PARAM(LMR_MIN_MOVECOUNT, 5, 1, 10, 1);

TUNABLE_PARAM(IIR_MIN_DEPTH, 6, 2, 9, 1);

TUNABLE_PARAM(LMP_MIN_MOVES_BASE, 2, 2, 8, 1);
TUNABLE_PARAM(LMP_DEPTH_SCALE, 1, 1, 10, 1);

TUNABLE_PARAM(MIN_ASP_WINDOW_DEPTH, 6, 3, 8, 1);
TUNABLE_PARAM(INITIAL_ASP_WINDOW, 40, 8, 64, 4);
TUNABLE_PARAM(ASP_WIDENING_FACTOR, 3, 1, 32, 2);