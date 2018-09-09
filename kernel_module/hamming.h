#ifndef _HAMMING_H_
#define _HAMMING_H_

#include <linux/rwsem.h>

// there's no way these kernels aren't already defined in the Linux source tree
#define HAMMING_CLEAR(x, y) x &= ~(((__int128)1) << y)
#define HAMMING_GET(x, y) !!(x & ((__int128_t)1) << y)
#define HAMMING_SET(x, y, z) HAMMING_CLEAR(x, y);x |= (((__int128)z) << y)
#define HAMMING_CLEAR_MEM(x) memset(&x, 0, sizeof(x))

typedef __int128 hamming_row_t;

/**
 * \brief Definition of error correcting stack
 *
 * You can choose a frontend and a backend for access. You can use any
 * combination of these, but the following are intended to be used
 *   - block_io and bin_tree
 *   - block_io and block_io
 *   - frontswap and bin_tree
 *
 * You *could* use frontswap and block_io, but this might incur more overhead
 * than just swapping via block io directly. It might make more sense if we
 * compress pages as well, but for now we don't
 */
typedef struct{
    struct{
        enum{
            FRONT_BLOCK_IO,
            FRONT_FRONTSWAP
        } mode;
        union{
            struct{
                struct request_queue *queue;
                struct gendisk *disk;
            } block_io;
            
            struct{
                unsigned swap_id;
            } frontswap;
        };
    } frontend;
    
    struct{
        enum{
            BACK_BIN_TREE,
            BACK_BLOCK_IO
        } mode;
        union{
            struct{
            
            } bin_tree;

            struct{
                // Unwritten right now, but forwards requests to backing storage
            } block_io;
        };
    } backend;
	struct rw_semaphore lock;
} hamming_t;

typedef struct{
    u64 time_micro_s;
    u64 addr;
} hamming_error_t;

// Paste HDD sectors back to back, and address them as offsets in a 4K page
#define SECTOR_SHIFT 9 // 512
#define SECTOR_SIZE (1 << SECTOR_SHIFT)
#define PAGE_SHIFT 12 // $K
// PAGE_SIZE is already defined in SLAB/SLUB/whatever
#define SECTORS_PER_PAGE_SHIFT (1 << (PAGE_SHIFT-SECTOR_SHIFT)) // 8 sectors per page 

#include "hamming_fast_logic.h"
#include "hamming_fast_logic_simple.h"

// TODO: actually make this through the sysfs, maybe allow dynamic increasing?
#define SECTOR_COUNT (1024*1024*2)

#endif
