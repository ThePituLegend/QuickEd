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
#include <stdbool.h>

typedef enum {
    QUICKED,
    WINDOWED,
    BANDED,
    ERA,
    H_ERA
} quicked_algo_t;

typedef struct quicked_params_t {
    quicked_algo_t algo;
    bool only_score;
    int bandwidth;
} quicked_params_t;

typedef struct quicked_aligner_t {
    quicked_params_t params;
    mm_allocator_t *mm_allocator;
    int score;
    char* cigar;
} quicked_aligner_t;

typedef enum {
    QUICKED_OK = 0,
    QUICKED_ERROR,          // Default error code
    QUICKED_UNKNOWN_ALGO,   // Provided algorithm is not supported

    // Development codes
    QUICKED_UNIMPLEMENTED,  // function declared but not implemented
    QUICKED_WIP,            // function implementation in progress
} quicked_status_t;

quicked_params_t quicked_default_params();
quicked_status_t quicked_new(
    quicked_aligner_t *aligner,
    quicked_params_t params
);
quicked_status_t quicked_free(
    quicked_aligner_t *aligner
);
quicked_status_t quicked_align(
    quicked_aligner_t *aligner,
    char* const pattern, const int pattern_len,
    char* const text, const int text_len
);

#endif // QUICKED_H
