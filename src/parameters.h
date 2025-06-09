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
constexpr std::array<std::array<double, 3>, 16> lmrL1 = {{     
                                                    {{0.3623902499675751, 0.3144463896751404, 0.023929506540298462}}, 
                                                    {{0.007864600978791714, 0.2516283392906189, -0.44610023498535156}}, 
                                                    {{-0.504273533821106, 0.01018944475799799, 0.46326741576194763}}, 
                                                    {{-0.29677265882492065, -0.49615031480789185, 0.5099984407424927}}, 
                                                    {{0.0031460586469620466, 0.4050731956958771, -0.03772173449397087}}, 
                                                    {{0.5303776264190674, 0.29247745871543884, 0.3152061998844147}}, 
                                                    {{-0.27401530742645264, -0.2804816663265228, 0.4145714044570923}}, 
                                                    {{0.1524629443883896, 0.180326446890831, 0.46183687448501587}}, 
                                                    {{0.38444751501083374, 0.1941826492547989, 0.20932349562644958}}, 
                                                    {{-0.2431570589542389, 0.41698524355888367, 0.43238911032676697}}, 
                                                    {{-0.5406186580657959, 0.1165563091635704, -0.15642938017845154}}, 
                                                    {{0.11602570116519928, -0.05724407732486725, 0.5666800737380981}}, 
                                                    {{-0.1490844488143921, -0.1814582645893097, 0.024552464485168457}}, 
                                                    {{0.16304492950439453, 0.3403719663619995, 0.21795888245105743}}, 
                                                    {{-0.14276517927646637, 0.09926635026931763, 0.5316518545150757}}, 
                                                    {{-0.36420968174934387, -0.5487035512924194, 0.4918321371078491}}
                                                }};

constexpr std::array<double, 16> lmrL1Bias = {-0.7126670479774475, 0.5776053667068481, -0.4152193069458008, -0.10518831014633179, -0.4245499074459076, 0.5465441346168518, -0.2940490245819092, -0.23332254588603973, 0.05184550955891609, -0.15206441283226013, -0.21799427270889282, -0.020012082532048225, 0.5756889581680298, 0.5913314819335938, 0.28221529722213745, 0.4279063940048218};
constexpr std::array<double, 16> lmrL2 = {-0.19277064502239227, 0.07518932223320007, 0.13103719055652618, 0.2292529046535492, -0.137904554605484, 0.04708902910351753, -0.161728173494339, -0.07970955967903137, 0.03277639299631119, 0.0017556932289153337, -0.029917337000370026, 0.08397520333528519, 0.04029238224029541, 0.26096808910369873, 0.061248671263456345, 0.15540632605552673};
constexpr double lmrOutputBias = -0.046497244387865067;
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