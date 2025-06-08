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

// Random big num
#ifdef _MSC_VER
    #include <__msvc_int128.hpp>
using u128 = std::_Unsigned128;
#else
using u128 = unsigned __int128;
#endif

//#define TUNE
//#define STORE_LMR_DATA

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


// History Constants
constexpr int16_t MAX_HISTORY = 16383;
const int16_t DEFAULT_HISTORY = 0;
constexpr int CORR_HIST_ENTRIES = 16384;
constexpr int MAX_CORR_HIST = 1024;
// NNUE Parameters
constexpr int16_t HL_N = 512;
constexpr int16_t QA = 255;
constexpr int16_t QB = 64;
constexpr int16_t NNUE_SCALE = 400;
constexpr int OUTPUT_BUCKETS = 8; 

// Lmr Net
constexpr std::array<std::array<double, 3>, 8> lmrL1 = {{     
                                                    {{-0.3925487995147705, -0.13486909866333008, -0.5493049025535583}}, 
                                                    {{-0.44353175163269043, 0.21422043442726135, -0.478349894285202}}, 
                                                    {{-0.08130542933940887, 0.36271312832832336, 0.4222992956638336}}, 
                                                    {{0.1768171638250351, -0.5785887837409973, 0.5442942380905151}}, 
                                                    {{-0.38667336106300354, 0.0864550843834877, -0.21922965347766876}}, 
                                                    {{-0.048668861389160156, -0.11039042472839355, 0.05080759525299072}}, 
                                                    {{-0.45028120279312134, 0.47515439987182617, -0.5792977809906006}}, 
                                                    {{-0.4849909842014313, 0.28225675225257874, -0.48005855083465576}}
                                                }};

constexpr std::array<double, 8> lmrL1Bias = {0.09951072931289673, 0.22508923709392548, -0.09015923738479614, 0.024610210210084915, 0.6090615391731262, -0.4105454981327057, -0.6159040331840515, 0.13162465393543243};
constexpr std::array<double, 8> lmrL2 = {0.21764466166496277, 0.22863060235977173, 0.3181397616863251, -0.22102832794189453, -0.2097621113061905, -0.08124381303787231, -0.24103787541389465, -0.1930999457836151};
constexpr double lmrOutputBias = 0.3846595585346222;
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
TUNABLE_PARAM(RFP_MARGIN, 76, 30, 100, 8);
TUNABLE_PARAM(RFP_MAX_DEPTH, 6, 4, 10, 1);

TUNABLE_PARAM(NMP_BASE_REDUCTION, 4, 2, 5, 1);
TUNABLE_PARAM(NMP_REDUCTION_SCALE, 4, 3, 6, 1);
TUNABLE_PARAM(NMP_EVAL_SCALE, 210, 50, 300, 10);

TUNABLE_PARAM(SE_MIN_DEPTH, 7, 4, 10, 1);
TUNABLE_PARAM(SE_BETA_SCALE, 31, 8, 64, 1);
TUNABLE_PARAM(SE_DOUBLE_MARGIN, 22, 0, 40, 2);

TUNABLE_PARAM(LMR_BASE_QUIET, 139, -50, 200, 5);
TUNABLE_PARAM(LMR_DIVISOR_QUIET, 278, 150, 350, 5);
TUNABLE_PARAM(LMR_BASE_NOISY, 20, -50, 200, 5);
TUNABLE_PARAM(LMR_DIVISOR_NOISY, 331, 150, 350, 5);
TUNABLE_PARAM(LMR_MIN_DEPTH, 1, 1, 8, 1);
TUNABLE_PARAM(LMR_MIN_MOVECOUNT, 4, 1, 10, 1);
TUNABLE_PARAM(LMR_HIST_DIVISOR, 8192, 4096, 16385, 650);

TUNABLE_PARAM(IIR_MIN_DEPTH, 5, 2, 9, 1);

TUNABLE_PARAM(LMP_MIN_MOVES_BASE, 2, 2, 8, 1);
TUNABLE_PARAM(LMP_DEPTH_SCALE, 1, 1, 10, 1);

TUNABLE_PARAM(SEE_PRUNING_SCALAR, -90, -128, -16, 16)

TUNABLE_PARAM(MIN_ASP_WINDOW_DEPTH, 4, 3, 8, 1);
TUNABLE_PARAM(INITIAL_ASP_WINDOW, 37, 8, 64, 4);
TUNABLE_PARAM(ASP_WIDENING_FACTOR, 3, 1, 32, 2);