#ifndef HAMMING_FAST_H
#define HAMMING_FAST_H

#include <unistd.h>
#include <memory.h>
#include <signal.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <time.h>
#include <stdio.h>


#define CLEAR(x, y) x &= ~(((__int128)1) << y)
#define GET(x, y) !!(x & ((__int128_t)1) << y)
#define SET(x, y, z) CLEAR(x, y);x |= (((__int128)z) << y)
#define CLEAR_MEM(x) memset(&x, 0, sizeof(x))

// defined casting
#define CLEAR_C(x, y, cast) x &= ~(((cast)1) << y)
#define GET_C(x, y, cast) !!(x & ((cast)1) << y)
#define SET_C(x, y, z, cast) CLEAR(x, y);x |= (((cast)z) << y)

typedef __int128 row_t;

#endif
