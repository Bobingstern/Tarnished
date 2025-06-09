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
                                                    {{-0.1792939156293869, -0.015294664539396763, 0.07458742707967758}}, 
                                                    {{0.0945434495806694, -0.28110694885253906, 0.04953126981854439}}, 
                                                    {{0.45708417892456055, -0.4402245283126831, -0.37472549080848694}}, 
                                                    {{0.41731590032577515, 0.42240405082702637, 0.271921843290329}}, 
                                                    {{0.5660421848297119, 0.18713919818401337, -0.49073079228401184}}, 
                                                    {{0.39020854234695435, 0.18939714133739471, -0.4363926947116852}}, 
                                                    {{0.47396552562713623, 0.07166249305009842, 0.4086728096008301}}, 
                                                    {{-0.490583211183548, 0.5328560471534729, -0.02115262672305107}}, 
                                                    {{0.19547879695892334, -0.40085089206695557, 0.06472133100032806}}, 
                                                    {{-0.2547081410884857, -0.12704584002494812, -0.2994838356971741}}, 
                                                    {{0.062424302101135254, -0.5114843845367432, 0.45368075370788574}}, 
                                                    {{0.25483638048171997, -0.5512445569038391, 0.5059328079223633}}, 
                                                    {{0.09226882457733154, 0.02306782454252243, -0.2893557548522949}}, 
                                                    {{-0.3498722314834595, -0.5043784976005554, 0.33007484674453735}}, 
                                                    {{-0.41780412197113037, 0.04538968950510025, -0.17224854230880737}}, 
                                                    {{0.43025633692741394, 0.49866607785224915, -0.3531742990016937}}
                                                }};

constexpr std::array<double, 16> lmrL1Bias = {0.18111519515514374, 0.398660808801651, -0.5997071266174316, 0.27098390460014343, 0.2063225954771042, -0.34929534792900085, -0.3020716607570648, -0.2425937056541443, -0.34270182251930237, 0.20981961488723755, 0.11763936281204224, 0.4806094765663147, -0.07540726661682129, -0.5349881649017334, 0.37898239493370056, -0.3058180510997772};
constexpr std::array<double, 16> lmrL2 = {-0.23776845633983612, 0.18420818448066711, 0.07062137126922607, 0.18941402435302734, -0.15676802396774292, 0.12132619321346283, 0.15345780551433563, -0.17960648238658905, 0.21329183876514435, -0.016847938299179077, -0.20636728405952454, 0.1685134917497635, -0.12936560809612274, 0.19832617044448853, -0.16727551817893982, 0.07758716493844986};
constexpr double lmrOutputBias = -0.1977691650390625;
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