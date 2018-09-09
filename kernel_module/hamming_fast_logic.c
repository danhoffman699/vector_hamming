#include "hamming.h"
#include "hamming_fast_logic.h"
#include "hamming_fast_logic_simple.h"

/**
 * \file hamming_fast_logic.c
 * \brief Set-based logic for Hamming codes
 */

/**
 * \brief Redundantly perform VHC
 *
 * 4096 bytes generates 256 rows of 128
 * Each iteration is ceil(log2(row_count + 1)) rows long
 * 256-> 9 -> 4
 *
 * Since ceil(log2(4 + 1)) == 3, and 4+(3*3) > 4*3, we just store the third
 * iteration three times redundantly
 *
 * \param[in] set		Structure containing all error correcting information
 * \param[in] board		4K page
 * \param[in] size		Length of page (?)
 */
void logic_set(hamming_code_set_t *set,
	      const hamming_row_t *board, int size){
	const int first_set_len = sizeof(set->first_set)/sizeof(hamming_row_t);
	const int second_set_len = sizeof(set->second_set[0])/sizeof(hamming_row_t);

	memset(set, 0, sizeof(*set));
	logic(set->first_set, first_set_len, board, size);
	logic(set->second_set[0], second_set_len,
	      set->first_set, first_set_len);
	memcpy(set->second_set[1], set->second_set[0], sizeof(set->second_set[0]));
	memcpy(set->second_set[2], set->second_set[0], sizeof(set->second_set[0]));
}


// Any errors detected with Hamming codes themselves are corrected here

/**
 * \brief Detect and correct any errors
 *
 * 1. Verify that two copies of third level VHCs are equal, correct if possible
 * 2. Correct first level with second level codes
 *
 * NOTE: There is actually nothing meaningful for when the RAID I fails, since this was
 * written before we had access to proper error reporting measures. Oops and logging to
 * the sysfs with all information would be important
 *
 * NOTE: Also there's apparently no correcting from third set onto second set?
 */
static bool set_sanity_check(hamming_code_set_t *first_set){
	const int first_set_size = sizeof(first_set->first_set)/sizeof(first_set->first_set[0]); // 9
	const int second_set_raid_size = sizeof(first_set->second_set)/sizeof(first_set->second_set[0]); // 3
	const int second_set_real_size = sizeof(first_set->second_set[0])/sizeof(first_set->second_set[0][0]); // 4
	int a, b;

	for(a = 0;a < second_set_raid_size;a++){
		for(b = 0;b < second_set_raid_size;b++){
			if(a == b) continue;
			if(memcmp(first_set->second_set[a],
				  first_set->second_set[b],
				  second_set_real_size) != 0){
				printk(KERN_ERR "RAID I version of second Hamming code failed, this is recoverable, but pretty bad\n");
				/*
				  The lowest level Hamming code RAID storage
				  is corrupted. There's nothing we can do at
				  this point but pray
				 */
				return false;
			}
		}
	}
	// Compute second set codes from first set data,
	// correct errors from first_set

	correct(first_set->second_set[0], second_set_real_size,
		first_set->first_set, first_set_size);
	return true;
}

/**
 * \brief Find errors from two sets of codes
 *
 * Currently only operates on second layer to first (9 to 256), but
 * should also operate on third layer to second layer
 *
 * \param[in] first_set		First set of hamming codes, most likely stored version
 * \param[in] second_set	Second set of hamming codes, most likely new version
 * \param[out] iter			Row where error has occured (cast 4K to __int128)
 * \param[out] bit			Bit in row where flip needs to occur to fix
 * \param[in] iter_bit_size	Length of iter and bit arrays (max number of reportable errors)
 *
 * \return Negative numbers on failure, 0 otherwise
 */
int get_errors_set(hamming_code_set_t *first_set,
		   hamming_code_set_t *second_set,
		   int *iter, int *bit, int iter_bit_size){
	const int first_set_size = sizeof(first_set->first_set)/sizeof(first_set->first_set[0]);
	if(memcmp(first_set, second_set, sizeof(hamming_code_set_t)) == 0){
		//printf("no differences via memcmp, returning 0 errors\n");
		return 0;
	}
	if(set_sanity_check(first_set) == false){
		return -1;
	}
	if(set_sanity_check(second_set) == false){
		return -1;
	}

	memset(iter, 0, sizeof(*iter));
	memset(bit, 0, sizeof(*bit));

	return get_errors(first_set->first_set, second_set->first_set, first_set_size,
			  iter, bit, iter_bit_size);
}

/**
 * \brief Find and correct errors from two sets of codes
 *
 * Calls get_errors_set, iterates over bits with flip_bit_raw
 *
 * \param[in] first_set		First set of hamming codes, most likely stored version
 * \param[in] second_set	Second set of hamming codes, most likely new version
 * \param[in] board			4K page as a matrix
 * \param[in] size			Length of elements in board, most likely 256
 *
 * \return Number of reported errors
 */
int correct_set(hamming_code_set_t *first_set,
		hamming_code_set_t *second_set,
		hamming_row_t *board, int size){
	int iter[64];
	int bit[64];
	int error_count, i;

	error_count = get_errors_set(first_set, second_set, iter, bit, 64);
	for(i = 0;i < error_count;i++){
		printk(KERN_ERR "Detected error %d at iter %d bit %d\n", i, iter[i], bit[i]);
		flip_bit_raw(iter[i], bit[i], board, size);
	}
	return error_count;
}
