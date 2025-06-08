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
                                                    {{-0.0018285512924194336, -0.4374125599861145, 0.35105830430984497}}, 
                                                    {{-0.36871176958084106, -0.3499261140823364, -0.06330084800720215}}, 
                                                    {{-0.04602095112204552, 0.5338038206100464, -0.24202337861061096}}, 
                                                    {{0.4811429977416992, -0.037343818694353104, 0.38126128911972046}}, 
                                                    {{-0.43675941228866577, -0.3506455421447754, -0.20887580513954163}}, 
                                                    {{0.3220365047454834, -0.011284834705293179, 0.10828674584627151}}, 
                                                    {{-0.4257749021053314, 0.09431909024715424, 0.37681299448013306}}, 
                                                    {{0.16241663694381714, -0.38079556822776794, 0.18995589017868042}}, 
                                                    {{0.03749018535017967, 0.25666314363479614, 0.3983948528766632}}, 
                                                    {{-0.07711178809404373, 0.30444663763046265, -0.04032063111662865}}, 
                                                    {{-0.4866432547569275, -0.10867327451705933, 0.10489886999130249}}, 
                                                    {{-0.06076958030462265, 0.091817706823349, 0.3717750608921051}}, 
                                                    {{0.10567250847816467, 0.24475759267807007, -0.168201744556427}}, 
                                                    {{-0.011501033790409565, 0.10736294835805893, -0.40410035848617554}}, 
                                                    {{-0.0058468906208872795, -0.0047754948027431965, -0.06336136907339096}}, 
                                                    {{-0.16072168946266174, -0.05234050750732422, -0.4571489095687866}}
                                                }};

constexpr std::array<double, 16> lmrL1Bias = {-0.5465381741523743, -0.00017136335372924805, 0.5349605679512024, 0.2510116696357727, -0.4486464262008667, -0.02941187471151352, -0.09890563040971756, -0.29363709688186646, 0.2683265805244446, -0.42969122529029846, 0.37133532762527466, -0.4948996901512146, 0.5664936900138855, 0.0738128200173378, 0.4828307032585144, -0.17002558708190918};
constexpr std::array<double, 16> lmrL2 = {-0.12642160058021545, 0.045390188694000244, 0.06621022522449493, 0.22030600905418396, 0.1985178291797638, 0.25239330530166626, -0.23670785129070282, -0.19905966520309448, -0.1669514924287796, -0.0509912334382534, -0.10469156503677368, 0.08219161629676819, 0.23856580257415771, -0.24603135883808136, 0.18426761031150818, -0.11437809467315674};
constexpr double lmrOutputBias = 0.20966172218322754;
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