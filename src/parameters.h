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
                                                    {{0.6774160265922546, -0.043531954288482666, -0.1246015876531601}}, 
                                                    {{0.655936062335968, -0.039756134152412415, 0.5403717756271362}}, 
                                                    {{-0.09281107783317566, 0.13318023085594177, -0.04466184601187706}}, 
                                                    {{0.5636781454086304, 0.48197636008262634, -0.3358275294303894}}, 
                                                    {{-0.1899052858352661, -0.08583882451057434, 0.22689813375473022}}, 
                                                    {{-0.0036780834197998047, -0.38411498069763184, 0.2278926968574524}}, 
                                                    {{0.41155895590782166, 0.36646217107772827, -0.009545284323394299}}, 
                                                    {{-0.46980974078178406, 0.44033142924308777, 0.017131458967924118}}, 
                                                    {{-0.4079097509384155, 0.21306806802749634, 0.2053024023771286}}, 
                                                    {{0.7568074464797974, -0.06769168376922607, 0.38041019439697266}}, 
                                                    {{0.11907071620225906, -0.26970046758651733, -0.15831716358661652}}, 
                                                    {{0.08644095063209534, 0.12450022995471954, -0.08276890963315964}}, 
                                                    {{0.28571879863739014, 0.3897766172885895, -0.14200559258460999}}, 
                                                    {{-0.43079817295074463, 0.24398143589496613, 0.251343309879303}}, 
                                                    {{-0.40580815076828003, 0.4333743155002594, 0.3661898076534271}}, 
                                                    {{-0.2008572220802307, -0.36296525597572327, 0.03594762086868286}} 
                                                }};

constexpr std::array<double, 16> lmrL1Bias = {-0.2145245224237442, -0.26489484310150146, 0.33010444045066833, 0.1040043979883194, -0.10086888074874878, 0.4287986755371094, 0.6716374754905701, -0.09187372028827667, 0.10148216784000397, 0.19204139709472656, -0.11048462986946106, 0.5201015472412109, -0.08651719987392426, 0.3572400212287903, -0.5893167853355408, 0.1504085659980774};
constexpr std::array<double, 16> lmrL2 = {0.3457965850830078, 0.37343478202819824, -0.17153441905975342, 0.11844891309738159, -0.14604514837265015, -0.14000239968299866, -0.0997566282749176, -0.22799542546272278, -0.03898775205016136, 0.3602982461452484, 0.097664974629879, 0.036232057958841324, 0.15896499156951904, 0.10513880103826523, 0.06290530413389206, -0.040946632623672485};
constexpr double lmrOutputBias = -0.03137229382991791;
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