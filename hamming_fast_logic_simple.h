#ifndef HAMMING_FAST_LOGIC_SIMPLE_H
#define HAMMING_FAST_LOGIC_SIMPLE_H
#include "hamming_fast_logic.h"
#include "hamming_fast.h"


// operators on rows themselves
extern int get_errors(const row_t *first_codes, const row_t *second_codes, int size,
		      int *iter, int *bit, int iter_bit_size);
extern void logic(row_t*, int, const row_t*, int);
extern int correct(row_t *new_codes, int new_codes_size,
		    row_t *board, int board_size);

// exposed to main function for sanity testing
extern void flip_bit_raw(int iter, int bit, row_t *board, int board_size);

#endif
