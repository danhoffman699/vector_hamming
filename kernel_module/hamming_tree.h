#ifndef _HAMMING_TREE_H_
#define _HAMMING_TREE_H_

#include "hamming.h"
#include "hamming_fast_logic.h"
#include "hamming_fast_logic_simple.h"

#include <linux/timekeeping.h>
#include <linux/ktime.h>

#define SECTOR_TO_PAGE(sector___) ((sector___ >> 3) << 3)
#define SECTOR_TO_CHUNK(sector___) (sector___ & 0b111)

// kernel throws undefined behavior when we shift the exact length

#define IS_SUBTREE(m, mpb, s) !((m ^ s) >> (32-mpb)) 

#define PAGE_PROCESSED_BITS 32

// max time we trust a verification before we reverify it
#define HAMMING_MAX_NS_DIFF 10*1000

typedef struct{
	u8 *data;
	u32 len; // must be 4096
	hamming_code_set_t code;
	u64 last_check;
} hamming_page_t; // page of allocated memory

typedef struct{
	void *child[2];
} hamming_node_t;

// atomic operations only
typedef struct{
	void **ptr;
	u32 id;
	u8 processed_bits; // children left until pages start
} hamming_subtree_t;

static hamming_subtree_t tree_head; // referenced for any global tree functions

/*
  All operations are generating subtrees from the master tree
  or another subtree

  Subtrees are passed into one of two conversion functions to
  give a hamming_node_t or hamming_page_t, depending on the
  depth of the subtree (based on processed_bits), mostly for
  sanity checking casts and what not.

  All elements are actually addressed as a double pointer,
  the pointer of the parent's pointer to the child, so deletions
  are safer and faster.
 */

// hamming_tree_subtree resolves as much as it can, only allowed to allocate if create is true
// typically cast to hamming_page_t
static int hamming_tree_resolve(hamming_subtree_t subtree, hamming_subtree_t *target, bool *create, int size);

// finds a pointer at a certain depth along a certain path
// tree_id is the path taken, processed bits is the depth down
// NOTE: internally, this only reads the first processed_bits of
// tree_id, so you don't have to sanitize inputs on that front

// Conversion functions to/from pointers
static hamming_page_t *hamming_tree_page_from_subtree(
	hamming_subtree_t subtree);

static hamming_node_t *hamming_tree_node_from_subtree(
	hamming_subtree_t subtree);

// TODO: should probably make this an __always_inline function
#define HAMMING_PAGE_LOGIC(page_ptr) logic_set(&(page_ptr->code), (__int128*)page_ptr->data, page_ptr->len/16)

static int hamming_tree_page_correct(
	hamming_page_t *page_ptr); // ran before any reading is done to a page

static void *hamming_tree_sector_from_page(
	hamming_page_t *page_ptr, u8 chunk);

// sets tree_head to valid runtime pointers
static void hamming_tree_init(void);

// DEPRECATED

// for testing and one off writes, we can use this instead
static void *hamming_tree_sector_simple(
	u32 tree_id, u8 chunk, bool create);

#endif
