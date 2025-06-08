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
                                                    {{-0.46570923924446106, 0.5002807974815369, -0.5262847542762756}}, 
                                                    {{0.26954972743988037, 0.11266694962978363, 0.19789566099643707}}, 
                                                    {{-0.2655464708805084, 0.18404780328273773, 0.1760142296552658}}, 
                                                    {{0.043302394449710846, 0.31050220131874084, 0.6575723886489868}}, 
                                                    {{0.28199338912963867, 0.001402961672283709, -0.2403767704963684}}, 
                                                    {{-0.5499678254127502, -0.054409969598054886, 0.4845624268054962}}, 
                                                    {{-0.573880672454834, -0.11249825358390808, 0.1734948754310608}}, 
                                                    {{-0.12265264987945557, -0.41817378997802734, 0.45563507080078125}}, 
                                                    {{0.06785344332456589, 0.23194344341754913, 0.33986374735832214}}, 
                                                    {{-0.7202810645103455, 0.3969692289829254, -0.42320752143859863}}, 
                                                    {{0.1794956922531128, 0.4810469150543213, -0.2618173062801361}}, 
                                                    {{0.3353876769542694, 0.2043372094631195, 0.008761695586144924}}, 
                                                    {{0.29066067934036255, 0.2913542091846466, 0.27932876348495483}}, 
                                                    {{0.5541471838951111, -0.000886740570422262, -0.3711511790752411}}, 
                                                    {{-0.05808520317077637, -0.2968452572822571, 0.15398883819580078}}, 
                                                    {{-0.414644330739975, -0.32500287890434265, -0.0010317564010620117}}
                                                }};

constexpr std::array<double, 16> lmrL1Bias = {0.34089258313179016, -0.34754514694213867, -0.4244527816772461, 0.49871256947517395, 0.5777698159217834, 0.33409982919692993, -0.4693334400653839, 0.4795428514480591, 0.5959958434104919, 0.11347764730453491, -0.31125086545944214, 0.04205683246254921, 0.5813536643981934, -0.09982529282569885, -0.45641323924064636, 0.3255341649055481};
constexpr std::array<double, 16> lmrL2 = {-0.1945863962173462, -0.06287435442209244, -0.14468026161193848, 0.0921882763504982, 0.18947330117225647, 0.13471288979053497, -0.19649282097816467, 0.19477516412734985, 0.0791914090514183, -0.20782971382141113, 0.2601405680179596, 0.08857258409261703, 0.013721700757741928, 0.13331778347492218, 0.17410188913345337, 0.09282428026199341};
constexpr double lmrOutputBias = 0.13548864424228668;
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