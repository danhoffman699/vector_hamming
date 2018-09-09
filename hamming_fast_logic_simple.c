#include "hamming_fast_logic_simple.h"
#include "hamming_fast_logic.h"
#include "hamming_fast.h"

static bool sanity_check_size_code_data(
	int code_size,
	int data_size){
	return (1 << code_size) >= data_size;
}

// exposed to main function for sanity testing
void flip_bit_raw(int iter, int bit, row_t *board, int board_size){
	if(iter > board_size){
		printf("iter is out of bounds\n");
		raise(SIGINT);
		return;
	}
	if(bit > (int)sizeof(board[0])*8){
		printf("bit is out of bounds\n");
		raise(SIGINT);
		return;
	}
	const bool old_val = GET(board[iter], bit);
	SET(board[iter], bit, !old_val); // SET clears first
}

// operators on individual chunks
void logic(row_t *codes, int code_length,
	   const row_t *data, int data_length){
	int a;
	int b;
	if(code_length > 255){
		printf("data_length is too long\n");
		raise(SIGINT);
	}
	for(a = 0;a < data_length;a++){
		const row_t tmp_data = data[a];
		for(b = 0;b < code_length;b++){
			if((1 << b) & a) codes[b] ^= tmp_data;
		}
	}
}

int get_errors(const row_t *old_codes, const row_t *new_codes, int size,
	       int *iter, int *bit, int iter_bit_size){
	int a, b, i, j;
	int cur_error = 0;
	for(i = 0;i < (int)sizeof(row_t)*8;i++){
		a = 0;
		b = 0;
		for(j = 0;j < size;j++){
			a |= GET(old_codes[j], i) << (j);
			b |= GET(new_codes[j], i) << (j);
		}
		if(a ^ b){
			iter[cur_error] = a ^ b;
			bit[cur_error] = i;
			cur_error++;
			if(cur_error == iter_bit_size){
				break;
			}

		}
	}
	return cur_error;
}

/*
  1. Perform basic sanity checks against the sizes (Hamming
  code of size N can't be represented in codes_size)
  2. Compute Hamming code of board_size
  3. Correct errors in board (assuming old codes are sanity
  checked against sub-codes by the caller)
  4. Return number of total bits corrected
 */

static row_t temp_codes[16];
static int temp_iter[256];
static int temp_bit[256];

int correct(row_t *codes, int codes_size,
	    row_t *board, int board_size){
	int error_count = 0;
	int i;
	CLEAR_MEM(temp_codes);
	CLEAR_MEM(temp_iter);
	CLEAR_MEM(temp_bit);
	
	if(sanity_check_size_code_data(codes_size, board_size) == false){
		printf("invalid code lengths %d and %d\n", codes_size, board_size);
		return -1;
	}
	if(codes_size > 16){
		printf("not enough room on the stack for codes\n");
		return -1;
	}
	logic(temp_codes, codes_size,
	      board, board_size);

	while((error_count = get_errors(codes, temp_codes, codes_size,
					temp_iter, temp_bit, 256)) > 0){
		for(i = 0;i < error_count;i++){
			flip_bit_raw(temp_iter[i], temp_bit[i], board, board_size);
		}
	}
	return error_count; // good enough
}
