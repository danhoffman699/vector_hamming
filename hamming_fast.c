#include "hamming_fast.h"
#include "hamming_fast_logic.h"
#include "hamming_fast_logic_simple.h"

#include <sys/time.h>

/*
  Prints the bit that was flipped in the error producing code
  Prints the bit that needs to be flipped back in the correction code
 */

/*
  For simplicity and ease of implementation, I am ignoring
  any horizontal Hamming code checks, which fits nicely
  into the current model, since any neighboring bits in
  the Hamming code are 128-bits apart.
 */

void error_detection_pseudocorrection(row_t *data, int data_size){
	hamming_code_set_t set, new_set;
	CLEAR_MEM(set);
	CLEAR_MEM(new_set);

	logic_set(&set, data, data_size);

	const int iter_tmp = (rand()%(data_size-1))+1;
	srand(time(NULL)*rand());
	const int bit_tmp = (rand()%(128-1))+1;
	srand(time(NULL)*rand());

	printf("changing %d %d\n", iter_tmp, bit_tmp);

	flip_bit_raw(iter_tmp, bit_tmp, data, data_size);

	if(GET(data[iter_tmp], bit_tmp) != 1){
		printf("something broke\n");
		raise(SIGINT);
	}

	logic_set(&new_set, data, data_size);

	int iter[16];
	int bit[16];
	int i;
	for(i = 0;i < 16;i++){
		iter[i] = 0;
		bit[i] = 0;
	}

	const int error_count = get_errors_set(&set, &new_set, iter, bit, 16);

	if(error_count != 1){
		printf("registered %d errors, not one, throwing SIGINT to investigate\n", error_count);
		raise(SIGINT);
	}else{
		printf("detected an error at %d %d, created at %d %d\n", iter[0], bit[0], iter_tmp, bit_tmp);
	}
}

static uint64_t get_time_micro_s(){
	struct timeval tv;
	gettimeofday(&tv, NULL);
	return 1000000ULL*tv.tv_sec + tv.tv_usec;
}

static void benchmark(row_t *board, int board_size){
	uint64_t sample_count = 0;
	long double rolling_avg = 0;
	uint64_t abs_start_time = get_time_micro_s();
	hamming_code_set_t code_set;
	while(true){
		CLEAR_MEM(*board);
		CLEAR_MEM(code_set);

	 	const uint64_t start_time = get_time_micro_s();
	 	logic_set(&code_set, board, board_size);
	 	const uint64_t end_time = get_time_micro_s();

		if(get_time_micro_s() - abs_start_time > 1000*1000){
			sample_count++;
			rolling_avg = ((rolling_avg * (sample_count-1)) + end_time-start_time)/sample_count; 
			printf("run took %ld microseconds, rolling average is %Lf\n", end_time-start_time, rolling_avg);
		} // optimize for caching why not
	}
}

static void error_checking(row_t *board, int data_size){
	int i;
	while(true){
		for(i = 0;i < data_size;i++){
			board[i] = 0;
		}
		error_detection_pseudocorrection(board, data_size);
	}
}

int main(){
	row_t board[256];
	benchmark(board, 256);
	//error_checking(board, 256);
	return 0;
}
