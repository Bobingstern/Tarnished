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
                                                    {{-0.2319921851158142, 0.22596530616283417, 0.49711692333221436}}, 
                                                    {{-0.0037612318992614746, 0.44572508335113525, 0.008533358573913574}}, 
                                                    {{0.770976722240448, -0.06024951487779617, 0.683750331401825}}, 
                                                    {{1.0921220779418945, 0.9200148582458496, 0.33704954385757446}}, 
                                                    {{0.14240020513534546, 0.1615046262741089, 0.3673427700996399}}, 
                                                    {{0.23121875524520874, 0.4939279556274414, -0.4490659832954407}}, 
                                                    {{0.36265501379966736, 0.6943320631980896, 0.3908872902393341}}, 
                                                    {{0.7760948538780212, 0.9242076277732849, 0.3427913784980774}}, 
                                                    {{0.34218746423721313, 0.8778374195098877, 0.7553759813308716}}, 
                                                    {{-0.111212819814682, 0.03816831111907959, 0.14104461669921875}}, 
                                                    {{-0.25260329246520996, -0.014385012909770012, -0.1123577281832695}}, 
                                                    {{-0.11841949075460434, -0.07899309694766998, -0.5177831053733826}}, 
                                                    {{-0.5722743272781372, -0.09164765477180481, -0.3056551516056061}}, 
                                                    {{0.43950754404067993, 0.5001590251922607, 0.26361778378486633}}, 
                                                    {{-0.01222229190170765, -0.010077455081045628, 0.05731331929564476}}, 
                                                    {{0.4873979389667511, 0.8227396607398987, 0.026821482926607132}} 
                                                }};

constexpr std::array<double, 16> lmrL1Bias = {0.6355663537979126, -0.3490642309188843, 0.6269572377204895, 1.1267374753952026, -0.45181500911712646, -0.11816823482513428, 0.4831523597240448, 0.7374720573425293, -0.04648815840482712, -0.1929212212562561, 0.09964527189731598, 0.5087054967880249, -0.5500286817550659, 0.5139747262001038, 0.7316944003105164, 0.714933454990387};
constexpr std::array<double, 16> lmrL2 = {-0.43467244505882263, 0.049210965633392334, -0.2152639925479889, -0.9488828778266907, -0.1761837899684906, 0.056003957986831665, -0.41229480504989624, -0.34661123156547546, -0.49171310663223267, 0.09638530015945435, 0.17931559681892395, -0.0597219243645668, 0.1441647708415985, -0.7249410152435303, -0.7038533091545105, -0.5420048236846924};
constexpr double lmrOutputBias = -0.0644063800573349;
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