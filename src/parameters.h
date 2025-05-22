#pragma once

#define MAX_PLY 125
#define BENCH_DEPTH 12
// History
constexpr int16_t MAX_HISTORY = 16383;
const int16_t DEFAULT_HISTORY = 0;
// NNUE Parameters
constexpr int16_t HL_N = 1024;
constexpr int16_t QA = 255;
constexpr int16_t QB = 64;
constexpr int16_t NNUE_SCALE = 400;
constexpr int OUTPUT_BUCKETS = 8; 

// Search Parameters
constexpr int RFP_MARGIN = 70;
constexpr int RFP_MAX_DEPTH = 6;

constexpr int NMP_BASE_REDUCTION = 3;
constexpr int NMP_REDUCTION_SCALE = 4;
constexpr int NMP_EVAL_SCALE = 200;

constexpr int SEE_ORDERING_MARGIN = -100;

constexpr int SE_MIN_DEPTH = 8;
constexpr int SE_BETA_SCALE = 32;
constexpr int SE_DOUBLE_MARGIN = 20;

// constexpr int HIST_PRUNING_DEPTH = 7;
// constexpr int HIST_PRUNING_MARGIN = -2048;
// constexpr int HIST_BASE_THRESHOLD = -380;
// constexpr int HIST_MULT_THRESHOLD = -2000;
// constexpr int HIST_CAPTURE_BASE_THRESHOLD = -500;
// constexpr int HIST_CAPTURE_MULT_THRESHOLD = -1700;

constexpr int LMR_MIN_DEPTH = 2;

constexpr int HISTORY_QUADRATIC_BONUS = 20;

constexpr int IIR_MIN_DEPTH = 6;

constexpr int LMP_MIN_MOVES_BASE = 2;

constexpr int MIN_ASP_WINDOW_DEPTH = 6;
constexpr int INITIAL_ASP_WINDOW = 40; 
constexpr int ASP_WIDENING_FACTOR = 3;