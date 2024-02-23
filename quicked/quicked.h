/*
 *                             The MIT License
 *
 * This file is part of QuickEdit library.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef QUICKED_H
#define QUICKED_H

#include "utils/include/mm_allocator.h"
#include "utils/include/profiler_timer.h"
#include <stdbool.h>

#define QUICKED_WINDOW_STAGES 2 // Number of window sizes to go through before doing banded
#define QUICKED_FAST_WINDOW_SIZE 2
#define QUICKED_FAST_WINDOW_OVERLAP 1
#define SCORE_ONLY true

typedef enum {
    QUICKED,
    WINDOWED,
    BANDED,
    HIRSCHBERG,
} quicked_algo_t;

typedef struct quicked_params_t {
    quicked_algo_t algo;
    unsigned int bandwidth;
    unsigned int windowSize;
    unsigned int overlapSize;
    unsigned int hewThreshold[QUICKED_WINDOW_STAGES];
    unsigned int hewPercentage[QUICKED_WINDOW_STAGES];
    bool onlyScore;
    bool forceScalar;
    bool external_timer;
} quicked_params_t;

typedef struct quicked_aligner_t {
    quicked_params_t* params;
    mm_allocator_t *mm_allocator;
    char* cigar;
    int score;
    // Profiling
    profiler_timer_t *timer;
    profiler_timer_t *timer_windowed_s;
    profiler_timer_t *timer_windowed_l;
    profiler_timer_t *timer_banded;
    profiler_timer_t *timer_align;
} quicked_aligner_t;

typedef enum quicked_status_t {
    QUICKED_OK = 0,
    QUICKED_ERROR,          // Default error code
    QUICKED_UNKNOWN_ALGO,   // Provided algorithm is not supported

    // Development codes
    QUICKED_UNIMPLEMENTED,  // function declared but not implemented
    QUICKED_WIP,            // function implementation in progress
} quicked_status_t;

quicked_params_t quicked_default_params(void);
quicked_status_t quicked_new(
    quicked_aligner_t *aligner,
    quicked_params_t *params
);
quicked_status_t quicked_free(
    quicked_aligner_t *aligner
);
quicked_status_t quicked_align(
    quicked_aligner_t *aligner,
    const char* pattern, const int pattern_len,
    const char* text, const int text_len
);

#endif // QUICKED_H
