#ifndef HAMMING_FAST_LOGIC_SIMPLE_H
#define HAMMING_FAST_LOGIC_SIMPLE_H
#include "hamming_fast_logic.h"
#include "hamming.h"


// operators on rows themselves
extern int get_errors(const hamming_row_t *first_codes, const hamming_row_t *second_codes, int size,
		      int *iter, int *bit, int iter_bit_size);
extern void logic(hamming_row_t*, int, const hamming_row_t*, int);
extern int correct(hamming_row_t *new_codes, int new_codes_size,
		    hamming_row_t *board, int board_size);

// exposed to main function for sanity testing
extern void flip_bit_raw(int iter, int bit, hamming_row_t *board, int board_size);

#endif
