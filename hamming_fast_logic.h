#ifndef HAMMING_FAST_LOGIC_H
#define HAMMING_FAST_LOGIC_H

#include "hamming_fast.h"

typedef struct{
	row_t first_set[9];
	row_t second_set[3][4];
} hamming_code_set_t;

// operators on hamming_code_sets (precoded with 256->9->4x3
extern int get_errors_set(hamming_code_set_t*, hamming_code_set_t*,
		      int *iter, int *bit, int iter_bit_size);
extern void logic_set(hamming_code_set_t*,
		      const row_t*, int);
extern int correct_set(hamming_code_set_t *first_set,
			hamming_code_set_t *second_set,
			row_t *board, int board_size);

/*
  All Hamming codes are stored vertically, since:

  1. By far the fastest way of computing Hamming codes (5 microseconds
     for computing and storing 128 Hamming codes vertically versus
     approx 1.6 milliseconds for computing 128 Hamming codes horizontally)

  2. Most efficient way of storing Hamming codes, as we can specify lengths
     of all the Hamming codes in bits and lose no storage to padding

  3. Computing vertical Hamming codes creates a situation where sequential
     flipped bits in physical memory map to different columns of Hamming
     codes, and assuming one strip per check, and the strip is less than 129
     bits long, it can be corrected properly. If the strip is less than 257
     bits long (and I can be bothered with implementing it), it can be detected
     but not corrected (probably log to kernel log, nothing we can only pray at
     this point)

  Vertical storage offloads converting vertical to horizontal only for 
  correcting invalid data (checks are done via memcmp)

  Hamming codes don't protect the first bit as implemented here, as
  (1 << c) & b is false for all b == 0, so we increment the position
  by one and live with an extra strip of bits per implementation.
  I'm not bothered with fixing this right now (if it can be fixed?),
  but more efficiency won't hurt.
  The first Hamming code is computed from the given data (4K of data)
  spread up into 128 256-bit long strips (vertical). This gives us
    ceil(log2((4096/16)+1)) == 9 bits long per strip

  The second Hamming code is computed from the first Hamming code, 
  to prevent flipping bits incorrectly on an invalid Hamming code.
  This gives us
    ceil(9+1) == 4 bits long per strip

  The second Hamming code is redundantly stored three times, since 
  redundantly storing subsequent codes three times while including
  the current one artificially increases the size.

  According to the math, this gives us about 8.2% storage overhead,
  and the effective memory throughput on my laptop is (probably) faster
  than the native throughput on the satellite (Cortex-A9 based)
 */


#endif
