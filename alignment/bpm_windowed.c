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


/*
 *  Wavefront Alignment Algorithms
 *  Copyright (c) 2017 by Santiago Marco-Sola  <santiagomsola@gmail.com>
 *
 *  This file is part of Wavefront Alignment Algorithms.
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * PROJECT: Wavefront Alignment Algorithms
 * AUTHOR(S): Santiago Marco-Sola <santiagomsola@gmail.com>
 * DESCRIPTION: Edit-Distance based BPM alignment algorithm
 */

#include "utils/commons.h"
#include "system/mm_allocator.h"
#include "alignment/bpm_windowed.h"
#include "utils/dna_text.h"

/*
 * Constants
 */
#define BPM_ALPHABET_LENGTH 4
#define BPM_W64_LENGTH UINT64_LENGTH
#define BPM_W64_SIZE   UINT64_SIZE
#define BPM_W64_ONES   UINT64_MAX
#define BPM_W64_MASK   (1ull<<63)

/*
 * Pattern Accessors
 */
#define BPM_PATTERN_PEQ_IDX(word_pos,encoded_character)   (((word_pos)*BPM_ALPHABET_LENGTH)+(encoded_character))
#define BPM_PATTERN_BDP_IDX(position,num_words,word_pos)  ((position)*(num_words)+(word_pos))
/*
 * Advance block functions (Improved)
 *   const @vector Eq,mask;
 *   return (Pv,Mv,PHout,MHout);
 */
#define BPM_ADVANCE_BLOCK(Eq,Pv,Mv,PHin,MHin,PHout,MHout) \
  /* Computes modulator vector {Xv,Xh} ( cases A&C ) */ \
  const uint64_t Xv = Eq | Mv; \
  const uint64_t _Eq = Eq | MHin; \
  const uint64_t Xh = (((_Eq & Pv) + Pv) ^ Pv) | _Eq; \
  /* Calculate Hout */ \
  uint64_t Ph = Mv | ~(Xh | Pv); \
  uint64_t Mh = Pv & Xh; \
  /* Account Hout that propagates for the next block */ \
  PHout = Ph >> 63; \
  MHout = Mh >> 63; \
  /* Hout become the Hin of the next cell */ \
  Ph <<= 1; \
  Mh <<= 1; \
  /* Account Hin coming from the previous block */ \
  Ph |= PHin; \
  Mh |= MHin; \
  /* Finally, generate the Vout */ \
  Pv = Mh | ~(Xv | Ph); \
  Mv = Ph & Xv


__attribute__ ((noinline)) 
void bpm_advance_block_func(const uint64_t Eq, 
    uint64_t *Pv, 
    uint64_t *Mv, 
    const uint64_t PHin, 
    const uint64_t MHin, 
    uint64_t *PHout, 
    uint64_t *MHout){ 
  /* Computes modulator vector {Xv,Xh} ( cases A&C ) */ 
  const uint64_t Xv = Eq | *Mv; 
  const uint64_t _Eq = Eq | MHin; 
  const uint64_t Xh = (((_Eq & *Pv) + *Pv) ^ *Pv) | _Eq; 
  /* Calculate Hout */ 
  uint64_t Ph = *Mv | ~(Xh | *Pv); 
  uint64_t Mh = *Pv & Xh; 
  /* Account Hout that propagates for the next block */ 
  *PHout = Ph >> 63; 
  *MHout = Mh >> 63; 
  /* Hout become the Hin of the next cell */ 
  Ph <<= 1; 
  Mh <<= 1; 
  /* Account Hin coming from the previous block */ 
  Ph |= PHin; 
  Mh |= MHin; 
  /* Finally, generate the Vout */ 
  *Pv = Mh | ~(Xv | Ph); 
  *Mv = Ph & Xv;
}
/*
 * Setup
 */
void windowed_pattern_compile(
    windowed_pattern_t* const windowed_pattern,
    char* const pattern,
    const int pattern_length,
    mm_allocator_t* const mm_allocator) {
  // Calculate dimensions
  const uint64_t pattern_num_words64 = DIV_CEIL(pattern_length,BPM_W64_LENGTH);
  const uint64_t PEQ_length = pattern_num_words64*BPM_W64_LENGTH;
  const uint64_t pattern_mod = pattern_length%BPM_W64_LENGTH;
  // Init fields
  windowed_pattern->pattern = pattern;
  windowed_pattern->pattern_length = pattern_length;
  windowed_pattern->pattern_num_words64 = pattern_num_words64;
  windowed_pattern->pattern_mod = pattern_mod;
  // Allocate memory
  const uint64_t aux_vector_size = pattern_num_words64*BPM_W64_SIZE;
  const uint64_t PEQ_size = BPM_ALPHABET_LENGTH*aux_vector_size;
  const uint64_t score_size = pattern_num_words64*UINT64_SIZE;
  const uint64_t total_memory = PEQ_size + 3*aux_vector_size + 2*score_size + (pattern_num_words64+1)*UINT64_SIZE;
  void* memory = mm_allocator_malloc(mm_allocator,total_memory);
  windowed_pattern->PEQ = memory; memory += PEQ_size;
  windowed_pattern->P = memory; memory += aux_vector_size;
  windowed_pattern->M = memory; memory += aux_vector_size;
  windowed_pattern->level_mask = memory; memory += aux_vector_size;
  windowed_pattern->score = memory; memory += score_size;
  windowed_pattern->init_score = memory; memory += score_size;
  windowed_pattern->pattern_left = memory;
  // Init PEQ
  memset(windowed_pattern->PEQ,0,PEQ_size);
  uint64_t i;
  for (i=0;i<pattern_length;++i) {
    const uint8_t enc_char = dna_encode(pattern[i]);
    const uint64_t block = i/BPM_W64_LENGTH;
    const uint64_t mask = 1ull<<(i%BPM_W64_LENGTH);
    windowed_pattern->PEQ[BPM_PATTERN_PEQ_IDX(block,enc_char)] |= mask;
  }
  for (;i<PEQ_length;++i) { // Padding
    const uint64_t block = i/BPM_W64_LENGTH;
    const uint64_t mask = 1ull<<(i%BPM_W64_LENGTH);
    uint64_t j;
    for (j=0;j<BPM_ALPHABET_LENGTH;++j) {
      windowed_pattern->PEQ[BPM_PATTERN_PEQ_IDX(block,j)] |= mask;
    }
  }
  // Init auxiliary data
  uint64_t pattern_left = pattern_length;
  const uint64_t top = pattern_num_words64-1;
  memset(windowed_pattern->level_mask,0,aux_vector_size);
  for (i=0;i<top;++i) {
    windowed_pattern->level_mask[i] = BPM_W64_MASK;
    windowed_pattern->init_score[i] = BPM_W64_LENGTH;
    windowed_pattern->pattern_left[i] = pattern_left;
    pattern_left = (pattern_left > BPM_W64_LENGTH) ? pattern_left-BPM_W64_LENGTH : 0;
  }
  for (;i<=pattern_num_words64;++i) {
    windowed_pattern->pattern_left[i] = pattern_left;
    pattern_left = (pattern_left > BPM_W64_LENGTH) ? pattern_left-BPM_W64_LENGTH : 0;
  }
  if (pattern_mod > 0) {
    const uint64_t mask_shift = pattern_mod-1;
    windowed_pattern->level_mask[top] = 1ull<<(mask_shift);
    windowed_pattern->init_score[top] = pattern_mod;
  } else {
    windowed_pattern->level_mask[top] = BPM_W64_MASK;
    windowed_pattern->init_score[top] = BPM_W64_LENGTH;
  }
}
void windowed_pattern_free(
    windowed_pattern_t* const windowed_pattern,
    mm_allocator_t* const mm_allocator) {
  mm_allocator_free(mm_allocator,windowed_pattern->PEQ);
}
void windowed_matrix_allocate(
    windowed_matrix_t* const windowed_matrix,
    const uint64_t pattern_length,
    const uint64_t text_length,
    mm_allocator_t* const mm_allocator,
    const int window_size) {
  // Parameters
  //const uint64_t num_words64 = DIV_CEIL(pattern_length,BPM_W64_LENGTH);
  const uint64_t num_words64 = window_size;
  // Allocate auxiliary matrix
  //const uint64_t aux_matrix_size = num_words64*UINT64_SIZE*(text_length+1); /* (+1 base-column) */
  const uint64_t aux_matrix_size = num_words64*UINT64_SIZE*(BPM_W64_LENGTH*window_size+1); /* (+1 base-column) */
  uint64_t* const Pv = (uint64_t*)mm_allocator_malloc(mm_allocator,aux_matrix_size);
  uint64_t* const Mv = (uint64_t*)mm_allocator_malloc(mm_allocator,aux_matrix_size);
  windowed_matrix->Mv = Mv;
  windowed_matrix->Pv = Pv;
  windowed_matrix->pos_v = pattern_length-1;
  windowed_matrix->pos_h = text_length-1;
  // CIGAR
  windowed_matrix->cigar = cigar_new(pattern_length+text_length);
  windowed_matrix->cigar->end_offset = pattern_length+text_length;
  windowed_matrix->cigar->begin_offset = pattern_length+text_length-1;
}
void windowed_matrix_free(
    windowed_matrix_t* const windowed_matrix,
    mm_allocator_t* const mm_allocator) {
  mm_allocator_free(mm_allocator,windowed_matrix->Mv);
  mm_allocator_free(mm_allocator,windowed_matrix->Pv);
  // CIGAR
  cigar_free(windowed_matrix->cigar);
}
/*
 * Edit distance computation using BPM
 */
void windowed_reset_search_cutoff(
    uint64_t* const P,
    uint64_t* const M,
    const uint64_t max_distance) {
  // Calculate the top level (maximum bit-word for cut-off purposes)
  const uint8_t y = (max_distance>0) ? (max_distance+(BPM_W64_LENGTH-1))/BPM_W64_LENGTH : 1;
  // Reset score,P,M
  uint64_t i;
  P[0]=BPM_W64_ONES;
  M[0]=0;
  for (i=1;i<y;++i) {
    P[i]=BPM_W64_ONES;
    M[i]=0;
  }
}
void windowed_compute_window(
    windowed_matrix_t* const windowed_matrix,
    windowed_pattern_t* const windowed_pattern,
    char* const text,
    const uint64_t text_length,
    uint64_t max_distance,
    const int window_size) {
  // Pattern variables
  const uint64_t* PEQ = windowed_pattern->PEQ;
  //const uint64_t num_words64 = windowed_pattern->pattern_num_words64;
  const uint64_t num_words64 = window_size;
  const uint64_t* const level_mask = windowed_pattern->level_mask;
  //int64_t* const score = windowed_pattern->score;
  uint64_t* const Pv = windowed_matrix->Pv;
  uint64_t* const Mv = windowed_matrix->Mv;
  //const uint64_t max_distance__1 = max_distance+1;
  windowed_reset_search_cutoff(Pv,Mv,BPM_W64_LENGTH*window_size);
  // Advance in DP-bit_encoded matrix
  uint64_t text_position;
  int64_t pos_v_fi = windowed_matrix->pos_v/UINT64_LENGTH;
  int64_t pos_h_fi = windowed_matrix->pos_h;

  int64_t pos_v = (pos_v_fi-(window_size-1) >= 0) ? pos_v_fi - (window_size-1) : 0;
  //if (pos_v < 0) pos_v = 0;
  int64_t pos_h = (pos_h_fi-UINT64_LENGTH*(window_size-1) >= 0) ? (pos_h_fi / UINT64_LENGTH) * UINT64_LENGTH-(window_size-1)*UINT64_LENGTH : 0;
  //if (pos_h < 0) pos_h = 0;

  int64_t steps_v = pos_v_fi - pos_v;
  int64_t steps_h = pos_h_fi - pos_h;


  //printf("\n\n----------------------------------------------------------\n");
  //printf("----------------------------------------------------------\n");
  //printf("(pos_v, pos_h) = (%ld,%ld)\n",pos_v, pos_h);
  //printf("(pos_v_fi, pos_h_fi) = (%ld,%ld)\n",pos_v_fi, pos_h_fi);

  for (text_position=0;text_position<=steps_h;++text_position) {
    // Fetch next character
    const uint8_t enc_char = dna_encode(text[text_position+pos_h]);
    // Advance all blocks
    uint64_t i,PHin=1,MHin=0,PHout,MHout;
    for (i=0;i<=steps_v;++i) {
      /* Calculate Step Data */
      const uint64_t bdp_idx = BPM_PATTERN_BDP_IDX(text_position,num_words64,i);
      const uint64_t next_bdp_idx = bdp_idx+num_words64;
      uint64_t Pv_in = Pv[bdp_idx];
      uint64_t Mv_in = Mv[bdp_idx];
      const uint64_t mask = level_mask[i+pos_v];
      const uint64_t Eq = PEQ[BPM_PATTERN_PEQ_IDX(i+pos_v,enc_char)];
      /* Compute Block */

      //printf("\n\n-----------------------------\n");
      //printf("(text_position,i)= (%ld,%ld)\n",text_position,i);
      //printf("(text_pos,pattern_pos)= (%ld,%ld)\n",text_position+pos_h,i+pos_v);
      //printf("enc_char, Eq: %ld, %lx\n",enc_char, Eq);
      //printf("mask: %lx\n", mask);
      //printf("Pv_in: %lx \nMv_in: %lx\n", Pv_in, Mv_in);
      //printf("PHin: %lx \nMHin: %lx\n", PHin, MHin);

      BPM_ADVANCE_BLOCK(Eq,Pv_in,Mv_in,PHin,MHin,PHout,MHout);
      //bpm_advance_block_func(Eq,&Pv_in,&Mv_in,PHin,MHin,&PHout,&MHout);
      //printf("Pv_out: %lx \nMv_out: %lx\n", Pv_in, Mv_in);
      //printf("PHout: %lx \nMHout: %lx\n", PHout, MHout);

      /* Adjust score and swap propagate Hv */
      //score[i] += PHout-MHout;
      Pv[next_bdp_idx] = Pv_in;
      Mv[next_bdp_idx] = Mv_in;
      PHin=PHout;
      MHin=MHout;
    }
  }
  //printf("\n\n----------------------------------------------------------\n");
  //printf("----------------------------------------------------------\n");
}


void windowed_backtrace_window(
    windowed_matrix_t* const windowed_matrix,
    const windowed_pattern_t* const windowed_pattern,
    char* const text,
    const int window_size, 
    const int overlap_size) {
  // Parameters
  char* const pattern = windowed_pattern->pattern;
  const uint64_t* const Pv = windowed_matrix->Pv;
  const uint64_t* const Mv = windowed_matrix->Mv;
  char* const operations = windowed_matrix->cigar->operations;
  int op_sentinel = windowed_matrix->cigar->begin_offset;
  // Retrieve the alignment. Store the match
  const uint64_t num_words64 = window_size;
  int64_t h = windowed_matrix->pos_h;
  int64_t v = windowed_matrix->pos_v;
  int64_t h_min = windowed_matrix->pos_h-UINT64_LENGTH*(window_size - 1) > 0 ? ((windowed_matrix->pos_h-(window_size - 1)*UINT64_LENGTH)/UINT64_LENGTH)*UINT64_LENGTH : 0;
  int64_t h_overlap = windowed_matrix->pos_h-UINT64_LENGTH*(window_size - overlap_size - 1) > 0 ? ((windowed_matrix->pos_h-(window_size - overlap_size - 1)*UINT64_LENGTH)/UINT64_LENGTH)*UINT64_LENGTH : 0;
  int64_t v_min = windowed_matrix->pos_v-UINT64_LENGTH*(window_size - 1) > 0 ? ((windowed_matrix->pos_v-(window_size - 1)*UINT64_LENGTH)/UINT64_LENGTH)*UINT64_LENGTH : 0;
  int64_t v_overlap = windowed_matrix->pos_v-UINT64_LENGTH*(window_size - overlap_size - 1) > 0 ? ((windowed_matrix->pos_v-(window_size - overlap_size - 1)*UINT64_LENGTH)/UINT64_LENGTH)*UINT64_LENGTH : 0;

  //printf("\n\n----------------------------------------------------------\n");
  //printf("----------------------------------------------------------\n");
  //printf("(v, h) = (%ld,%ld)\n",v, h);
  //printf("(v_overlap, h_overlap) = (%ld,%ld)\n",v_overlap, h_overlap);
  //printf("(v_min, h_min) = (%ld,%ld)\n",v_min, h_min);

  while (v >= v_overlap && h >= h_overlap){
    const uint8_t block = (v-v_min) / UINT64_LENGTH;
    const uint64_t bdp_idx = BPM_PATTERN_BDP_IDX((h-h_min+1),num_words64,block);
    const uint64_t mask = 1L << (v % UINT64_LENGTH);
    // CIGAR operation Test
    //printf("block = %ld\n", block);
    //printf("bdp_idx = %ld\n", bdp_idx);
    //printf("Pv[bdp_idx] = %ld\n", Pv[bdp_idx]);
    //printf("Mv[(bdp_idx-num_words64)] = %ld\n", Mv[(bdp_idx-num_words64)]);

    if (Pv[bdp_idx] & mask) {
      operations[op_sentinel--] = 'D';
      --v;
    } else if (Mv[(bdp_idx-num_words64)] & mask) {
      operations[op_sentinel--] = 'I';
      --h;
    } else if ((text[h]==pattern[v])) {
      operations[op_sentinel--] = 'M';
      --h;
      --v;
    } else {
      operations[op_sentinel--] = 'X';
      --h;
      --v;
    }
  } 
  windowed_matrix->pos_h = h;
  windowed_matrix->pos_v = v;
  //printf("(end_v, end_h) = (%ld,%ld)\n",v, h);
  windowed_matrix->cigar->begin_offset = op_sentinel;
}
void windowed_compute(
    windowed_matrix_t* const windowed_matrix,
    windowed_pattern_t* const windowed_pattern,
    char* const text,
    const int text_length,
    const int max_distance,
    const int window_size, 
    const int overlap_size) {
  
  while (windowed_matrix->pos_v >= 0 && windowed_matrix->pos_h >= 0) {
    // Fill window (Pv,Mv)
    windowed_compute_window(
        windowed_matrix,windowed_pattern,
        text,text_length,max_distance,window_size);
    // Compute window backtrace
    windowed_backtrace_window(windowed_matrix,windowed_pattern,text,window_size,overlap_size);
  }

  int64_t h = windowed_matrix->pos_h;
  int64_t v = windowed_matrix->pos_v;
  char* const operations = windowed_matrix->cigar->operations;
  int op_sentinel = windowed_matrix->cigar->begin_offset;
  while (h>=0) {operations[op_sentinel--] = 'I'; --h;}
  while (v>=0) {operations[op_sentinel--] = 'D'; --v;}
  windowed_matrix->pos_h = h;
  windowed_matrix->pos_v = v;
  windowed_matrix->cigar->begin_offset = op_sentinel + 1;
}
