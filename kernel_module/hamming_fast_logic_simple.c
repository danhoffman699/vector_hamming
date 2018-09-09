#include "hamming_fast_logic_simple.h"
#include "hamming_fast_logic.h"
#include "hamming.h"

/**
 * \brief Sanity check code and data sizes
 * 
 * \param[in] code_size		Length of array of codes
 * \param[in] data_size		Length of array of data
 *
 * \return false on invalid, true otherwise
 */
static bool sanity_check_size_code_data(
	int code_size,
	int data_size){
	return (1 << code_size) >= data_size;
}

/**
 * \brief Flip bit in a page
 *
 * Performs basic bounds checking and flips the value.
 *
 * NOTE: this should probably return a value
 *
 * \param[in] iter			Row whose bit needs to flip
 * \param[in] bit			Offset of bit to flip inside row
 * \param[in] board			4K page as an array
 * \param[in] board_size	Length of board, most likely 256
 */
void flip_bit_raw(int iter, int bit, hamming_row_t *board, int board_size){
	bool old_val;
	if(iter > board_size){
		printk(KERN_ERR "iter is out of bounds (%d %d in board of length %d)\n", iter, bit, board_size);
		return;
	}
	if(bit > (int)sizeof(board[0])*8){
		printk(KERN_ERR "bit is out of bounds\n");
		return;
	}
	old_val = HAMMING_GET(board[iter], bit);
	HAMMING_SET(board[iter], bit, !old_val); // SET clears first
}

/**
 * \brief Perform the actual Hamming code computation
 *
 * This is actually stupid simple, it's just iterating over __int128 and XORing.
 * More space efficient systems could be made, but we get relatively nice protections
 * with this system, and we don't want to tag too much overhead to the block layer
 *
 * \param[in] codes			Destination of new code
 * \param[in] code_length	Length of array of codes
 * \param[in] data			Data to protect
 * \param[in] data_length	Length of data to protect
 */
void logic(hamming_row_t *codes, int code_length,
	   const hamming_row_t *data, int data_length){
	int a;
	int b;
	if(code_length > 255){
		printk(KERN_ERR "data_length is too long\n");
		// We can still operate here, but the data won't be represented
		// properly. We should still have some protection, but there should
		// never be a case where this is triggered, so something else should
		// be broken as well
	}
	for(a = 0;a < data_length;a++){
		const hamming_row_t tmp_data = data[a];
		for(b = 0;b < code_length;b++){
			if((1 << b) & a) codes[b] ^= tmp_data;
		}
	}
}

/**
 * \brief Fetch errors between two Hamming codes
 *
 * Writes position in array and bit to flip in iter and bit
 *
 * \param[in] old_codes		Old codes to compare errors against
 * \param[in] new_codes		New codes to compare errors against
 * \param[in] size			Length of both old_codes and new_codes
 * \param[out] iter			Array of position in array of data where error occured
 * \param[out] bit			Array of bit offsets in position where a flip is required
 * \param[in] iter_bit_size	Length of iter and bit arrays
 *
 * \return Number of errors found
 */

int get_errors(const hamming_row_t *old_codes, const hamming_row_t *new_codes, int size,
	       int *iter, int *bit, int iter_bit_size){
	int a, b, i, j;
	int cur_error = 0;
	for(i = 0;i < (int)sizeof(hamming_row_t)*8;i++){
		a = 0;
		b = 0;
		for(j = 0;j < size;j++){
			a |= HAMMING_GET(old_codes[j], i) << (j);
			b |= HAMMING_GET(new_codes[j], i) << (j);
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

static hamming_row_t temp_codes[16];
static int temp_iter[256];
static int temp_bit[256];

/**
 * \brief Correct all errors from an array of codes and the data
 *
 * Computes the latest version of hamming codes from the passed data and
 * calls get_errors.
 *
 * NOTE: We loop over get_errors, since it's possible to not report all errors, 
 * but still be able to make progress towards recovery
 *
 * \param[in] codes			Array of old codes from board
 * \param[in] codes_size	Length of old codes from board
 * \param[in] board			Data to correct
 * \param[in] board_size	Length of data to correct
 *
 * \return Number of errors corrected, otherwise zero
 */
int correct(hamming_row_t *codes, int codes_size,
	    hamming_row_t *board, int board_size){
	int error_count = 0;
	int i;

	memset(&temp_codes, 0, sizeof(temp_codes));
	memset(&temp_iter, 0, sizeof(temp_iter));
	memset(&temp_bit, 0, sizeof(temp_bit));

	if(sanity_check_size_code_data(codes_size, board_size) == false){
		printk(KERN_ERR "invalid code lengths %d and %d\n", codes_size, board_size);
		return -1;
	}
	if(codes_size > 16){
		printk(KERN_ERR "not enough room on the stack for codes\n");
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
