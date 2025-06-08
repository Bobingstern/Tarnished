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
constexpr std::array<std::array<double, 2>, 16> lmrL1 = {{     {{-0.35804399847984314, 0.696239709854126}}, 
                                                    {{0.29624658823013306, -0.6519079804420471}}, 
                                                    {{-0.6464449167251587, 0.834399938583374}}, 
                                                    {{0.43047672510147095, 0.14537352323532104}}, 
                                                    {{0.5221933722496033, 0.07002949714660645}}, 
                                                    {{-0.25790417194366455, 0.6612678170204163}}, 
                                                    {{-0.25359174609184265, 0.6175835728645325}}, 
                                                    {{0.45419377088546753, 0.9492920637130737}}, 
                                                    {{-0.7026352286338806, -0.43846386671066284}}, 
                                                    {{0.527226984500885, -0.6694098711013794}}, 
                                                    {{0.5031417012214661, 0.6003320813179016}}, 
                                                    {{0.511121928691864, 0.7466766834259033}}, 
                                                    {{-0.4245593547821045, -0.3932192921638489}}, 
                                                    {{-0.07279282808303833, 0.3511395752429962}}, 
                                                    {{-0.641230046749115, -0.1556299477815628}}, 
                                                    {{-0.3410291075706482, 0.6129454970359802}}
                                                }};

constexpr std::array<double, 16> lmrL1Bias = {0.8716380596160889, -0.04957550764083862, 1.4427037239074707, -0.5043133497238159, -0.10446232557296753, 1.193938136100769, 0.6204346418380737, 0.5711585283279419, -0.6592778563499451, -0.11934113502502441, 0.8494356274604797, 1.030152678489685, -0.009678840637207031, -0.05948689952492714, 0.9411636590957642, -0.25629574060440063};
constexpr std::array<double, 16> lmrL2 = {-0.6306613683700562, -0.24418264627456665, -0.4079231917858124, 0.045445650815963745, -0.14751151204109192, -0.49247801303863525, -0.8281254172325134, -0.8676120042800903, -0.10934007167816162, -0.06845226883888245, -0.9084001183509827, -0.5558039546012878, 0.17486226558685303, 0.20882803201675415, -0.6794273257255554, 0.00784534215927124};
constexpr double lmrOutputBias = -0.5447099804878235;
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