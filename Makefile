all:
	gcc -O0 -g -std=gnu89 -Wall -Wextra hamming_fast.c hamming_fast_logic.c hamming_fast_logic_simple.c -o fast_ver
