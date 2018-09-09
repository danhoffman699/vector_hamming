#include "hamming_fast.h"
#include "hamming_fast_logic.h"
#include "hamming_fast_logic_simple.h"

#define likely(x) __builtin_expect(!!(x), true)
#define unlikely(x) __builtin_expect(!!(x), false)

// operators on hamming_code_set_ts
void logic_set(hamming_code_set_t *set,
	      const row_t *board, int size){
	const int first_set_len = sizeof(set->first_set)/sizeof(row_t);
	const int second_set_len = sizeof(set->second_set[0])/sizeof(row_t);
	
	CLEAR_MEM(*set);
	logic(set->first_set, first_set_len, board, size);
	logic(set->second_set[0], second_set_len,
	      set->first_set, first_set_len);
	memcpy(set->second_set[1], set->second_set[0], sizeof(set->second_set[0]));
	memcpy(set->second_set[2], set->second_set[0], sizeof(set->second_set[0]));
}


// Any errors detected with Hamming codes themselves are corrected here

static bool set_sanity_check(hamming_code_set_t *first_set){
	const int first_set_size = sizeof(first_set->first_set)/sizeof(first_set->first_set[0]); // 9
	const int second_set_real_size = sizeof(first_set->second_set[0])/sizeof(first_set->second_set[0][0]); // 4

	// That loop is pretty darn retarded, if the first two match, continue normally
	// if they don't match, go full on retard voting mode
	bool valid = memcmp(first_set->second_set[0], first_set->second_set[1], second_set_real_size) == 0;
	if(valid == false){
		//const int second_set_raid_size = sizeof(first_set->second_set)/sizeof(first_set->second_set[0]); // 3
		printf("first crack at sanity checking second sets failed, TODO implements stupid weird bitwise voting system\n");
		/*
		  Vote on bits

		  Rule of Thumb: if the correct output doesn't match any of the three RAID inputs, then
		  return false (data is too scattered to be considered legitimate, since there is never
		  a situation where they all disagree), any false corrections at this point only
		  amplify the errors in the data we care about.
		*/
		printf("He's dead Jim\n");
		return false;
	}

	// Compute second set codes from first set data,
	// correct errors from first_set

	correct(first_set->second_set[0], second_set_real_size,
		first_set->first_set, first_set_size);
	return true;
}

int get_errors_set(hamming_code_set_t *first_set,
		   hamming_code_set_t *second_set,
		   int *iter, int *bit, int iter_bit_size){
	const int first_set_size = sizeof(first_set->first_set)/sizeof(first_set->first_set[0]);
	if(memcmp(first_set, second_set, sizeof(hamming_code_set_t)) == 0){
		printf("no differences via memcmp, returning 0 errors\n");
		return 0;
	}
	if(set_sanity_check(first_set) == false){
		return -1;
	}
	if(set_sanity_check(second_set) == false){
		return -1;
	}

	CLEAR_MEM(*iter);
	CLEAR_MEM(*bit);
	
	return get_errors(first_set->first_set, second_set->first_set, first_set_size,
			  iter, bit, iter_bit_size);
}

int correct_set(hamming_code_set_t *first_set,
		hamming_code_set_t *second_set,
		row_t *board, int size){
	int iter[64];
	int bit[64];
	int error_count, i;

	error_count = get_errors_set(first_set, second_set, iter, bit, 64);
	for(i = 0;i < error_count;i++){
		flip_bit_raw(iter[i], bit[i], board, size);
	}
	return error_count;
}
