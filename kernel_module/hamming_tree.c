#include "hamming_tree.h"
#include "hamming.h"

/**
 * \file hamming_tree.c
 * \brief Binary tree implementation
 *
 * I hand rolled my own binary tree implementation because why not
 */

static hamming_subtree_t tree_head;

/**
 * \brief Lookup a node in a tree
 *
 * This is a fairly complicated binary tree traversal that was supposed to allow for
 * partial traversals to generate subtrees that we can use for optimal prefetching,
 * since it would make sense for swap disks to optimize for locality.
 *
 * However, with current testing, I was getting over 90% the performance of loopback devices,
 * so I'm not concerned with increasing performance on that front until later
 *
 * \param[in] subtree		Head of tree to traverse, all fields valid
 * \param[in,out] target	Target location, id and processed_bytes valid on input, ptr valid on output
 * \param[in] create		Flag to allow for creation of new nodes
 *
 * \return -ENOMEM or 0 for success
 */
static int hamming_tree_resolve_raw(hamming_subtree_t subtree, hamming_subtree_t *target, bool create){
	u32 cur_mask, end_mask;
	
	target->ptr = NULL;
	cur_mask = (subtree.processed_bits == 32) ? 0 : (1 << (31-subtree.processed_bits));
	end_mask = (target->processed_bits == 32) ? 0 : (1 << (31-target->processed_bits));
	if(create){
        while(cur_mask != end_mask){
			subtree.ptr = &(((hamming_node_t*)(*subtree.ptr))->child[!!(target->id & cur_mask)]);
			if(unlikely(*(subtree.ptr) == NULL)){
				if(unlikely(cur_mask == 1)){ // last generation, hamming_node_t
					hamming_page_t *page_ptr;
					
					*(subtree.ptr) = kzalloc(sizeof(hamming_page_t), GFP_ATOMIC);
					if(unlikely(*(subtree.ptr) == NULL)){
						printk(KERN_ERR "can't allocate hamming_page_t\n");
						return -ENOMEM;
					}
					page_ptr = (hamming_page_t*)*(subtree.ptr);
					page_ptr->data = kzalloc(PAGE_SIZE, GFP_ATOMIC);
					if(unlikely(page_ptr->data == NULL)){
						printk(KERN_ERR "can't allocate page data\n");
						return -ENOMEM;
					}
					page_ptr->len = PAGE_SIZE;
				}else{
					*(subtree.ptr) = kzalloc(sizeof(hamming_node_t), GFP_ATOMIC);
					if(unlikely(*(subtree.ptr) == NULL)){
						printk(KERN_ERR "can't allocate hamming_node_t\n");
						return -ENOMEM;
					}
				}
			}
			cur_mask >>= 1;
			subtree.processed_bits++; // only for generate_lca
		}
	}else{
		while(cur_mask != end_mask){
			subtree.ptr = &(((hamming_node_t*)(*subtree.ptr))->child[!!(target->id & cur_mask)]);
			if(unlikely(*(subtree.ptr) == NULL)){
				return 0; // nothing we can do here
			}
			cur_mask >>= 1;
		}
	}	
	
	target->ptr = subtree.ptr;
	return 0;
}

/**
 * \brief Lookup multiple nodes in a tree
 *
 * Think of it like CoW for traversals, you share the path until you don't anymore, fork off from there.
 *
 * \param[in] subtree		Head of the tree to traverse, all fields valid
 * \param[in,out] target	Array of target locations, id and processed_bytes valid on input, ptr valid on output
 * \param[in] create		Array of create flags for targets
 * \param[in] size			Length of target and create arrays
 *
 * \return Zero
 */
static int hamming_tree_resolve(hamming_subtree_t subtree, hamming_subtree_t *target, bool *create, int size){
	int i;
	u32 cur_mask;
	hamming_subtree_t next_subtree;
	hamming_subtree_t *longest_subtree;
	u8 computing = 0;
	
	longest_subtree = target;
	for(i = 0;i < 8;i++){
		if(target[i].processed_bits > longest_subtree->processed_bits){
			longest_subtree = &(target[i]);
		}
	}
	cur_mask = (subtree.processed_bits == 32) ? 0 : (1 << (31-subtree.processed_bits));
	// traverse down the first tree on the main loop, resolve at the end
	while(cur_mask != 0 && computing != (1 << size)-1){ // non zero
		next_subtree.ptr = &(((hamming_node_t*)(*subtree.ptr))->child[!!(longest_subtree->id & cur_mask)]);
		next_subtree.processed_bits = subtree.processed_bits + 1;
		next_subtree.id = longest_subtree->id & (0xFFFFFFFF << (32-next_subtree.processed_bits)); // can compute directly from tree_id
		for(i = 0;i < size;i++){
			if(computing & (1 << i)){
				continue; // already resolved
			}
			if(IS_SUBTREE(next_subtree.id, next_subtree.processed_bits, target[i].id) == false){
				hamming_tree_resolve_raw(subtree, &(target[i]), create[i]); // has no retval, all errors set target[i] to NULL
				computing |= (1 << i);
			}
		}
		// next pointer is not used until the next iteration
		subtree = next_subtree;
		cur_mask >>= 1;
	}
	return 0;
}

/**
 * \brief Generate page from subtree
 *
 * If the node is a leaf node, then we interpret the pointer as a hamming_page_t
 *
 * \param[in] subtree		Subtree to pull page pointer from
 *
 * \return Page pointer on success, NULL on failure
 */
static hamming_page_t *hamming_tree_page_from_subtree(hamming_subtree_t subtree){
	if(unlikely(subtree.processed_bits != PAGE_PROCESSED_BITS)){
		printk(KERN_ERR "requested a page, but our processed_bits says we are a node (%d)\n", subtree.processed_bits);
		return NULL;
	}
	if(subtree.ptr == NULL){
		// often the case for reads where the page doesn't exist (no parent to reference from)
		return NULL;
	}
	return *(subtree.ptr);
}

/**
 * \brief Generate node from subtree
 * If the node is not a leaf node, then we interpret the pointer as a hamming_node_t
 *
 * \param[in] subtree 		Subtree to pull node pointer from
 *
 * \return Node pointer on success, NULL on failure
 */
static hamming_node_t *hamming_tree_node_from_subtree(hamming_subtree_t subtree){
	if(unlikely(subtree.processed_bits >= PAGE_PROCESSED_BITS)){
		printk(KERN_ERR "requested a node, but our processed_bits say we are a page or out of bounds (%d)\n", subtree.processed_bits);
		return NULL;
	}
	if(unlikely(subtree.ptr == NULL)){
		printk(KERN_ERR "subtree.ptr is a NULL\n");
		return NULL;
	}
	return *(subtree.ptr);
}

/**
 * \brief Pull sector pointer from page
 *
 * Returns an offset inside hamming_page_t where the specified sector can be found. This
 * just sanity checks and returns page_ptr->data + (chunk * SECTOR_SIZE)
 *
 * \param[in] page_ptr		Page pointer to pull sector from
 * \param[in] chunk			Number of sectors into page pointer
 *
 * \return Pointer to sector on success, NULL on failure
 */
static void *hamming_tree_sector_from_page(
	hamming_page_t *page_ptr, u8 chunk){
	if(unlikely(page_ptr == NULL)){
		printk(KERN_ERR "page_ptr is NULL\n");
		return NULL;
	}
	if(unlikely(page_ptr->len != PAGE_SIZE)){
		printk(KERN_ERR "page_ptr->len isn't PAGE_SIZE, fix this (%d)\n", page_ptr->len);
		return NULL;
	}
	if(unlikely(page_ptr->data == NULL)){
		printk(KERN_ERR "page_ptr->data is NULL, fix this\n");
		return NULL;
	}
	if(unlikely(chunk >= 8)){
		printk(KERN_ERR "chunk is out of bounds\n");
		return NULL;
	}
	return page_ptr->data + (chunk * SECTOR_SIZE);
}

/**
 * \brief Find sector in tree
 *
 * Given a tree_id and a chunk, traverse the tree and return the proper sector
 * information. This is often used to handle BIO requests. 
 *
 * For simplicity (for now), error correction is done when create is false (i.e.
 * when read operations are going on) on the entire page, if the last check time
 * is less than a second ago
 *
 * \param[in] tree_id		Traversal to take down tree, pulled from SECTOR_TO_PAGE
 * \param[in] chunk			Offset in page for sector, pulled from SECTOR_TO_CHUNK
 * \param[in] create		True to create sector if it doesn't exist
 *
 * \return Pointer to sector on success, NULL on failure
 */
static void *hamming_tree_sector_simple(
	u32 tree_id, u8 chunk, bool create){
	hamming_subtree_t subtree;
	hamming_page_t *page_ptr;

	subtree.processed_bits = 32; // at the 32nd depth, one node
	subtree.id = tree_id;

	if(unlikely(hamming_tree_resolve_raw(tree_head, &subtree, create) < 0)){
		printk(KERN_ERR "cannot create subtree\n");
		return NULL;
	}

	if(subtree.ptr == NULL){ // often the case for reads
		return NULL;
	}

	page_ptr = hamming_tree_page_from_subtree(subtree);
	if(unlikely(page_ptr == NULL)){
		if(create){
			printk(KERN_ERR "hamming_page_from_subtree returned NULL, check the binary tree functions\n");
		}
		return NULL;
	}
    
	return hamming_tree_sector_from_page(page_ptr, chunk);
}

/**
 * \brief Run error correction on a page
 *
 * This runs the vectorized ECC over an entire page. To keep sizes efficient, and because this was
 * originally modelled after 4K pages of RAM, not 512 sectors on disk, you can only run ECC operations
 * on 8 sectors at a time.
 * 
 * \param[in] page_ptr		Pointer to page to correct
 *
 * \return Number of errors detected
 */
static int hamming_tree_page_correct(hamming_page_t *page_ptr){
	int retval;
	ktime_t cur_time;
	cur_time = ktime_get();

	if(cur_time - page_ptr->last_check){
		int i;
		hamming_code_set_t new_code_set;
		logic_set(&new_code_set, (__int128*)page_ptr->data, page_ptr->len/16);
		retval = correct_set(&new_code_set, &(page_ptr->code), (__int128*)page_ptr->data, page_ptr->len/16);
		page_ptr->last_check = cur_time;
	}
	return retval;
}

static hamming_node_t tree_node;
static hamming_node_t *tree_ptr;

/**
 * \brief Initialize binary tree
 * 
 * I had some crazy idea of doing a double pointer traversal to optimize deletions with a simpler
 * code path (pointer to parent's pointer to child), and that's why we have tree_node and tree_ptr.
 * It works, but hot deletions from the tree isn't something implemented yet, so no way to show
 * whether that was a good choice or not currently.
 */
static void hamming_tree_init(void){
	if(tree_head.ptr != NULL){
		printk(KERN_ERR "tree started with a non-NULL, something fishy is happening\n");		
	}
	tree_ptr = &tree_node;
	tree_head.ptr = (void**)&tree_ptr;
	tree_head.processed_bits = 0;
	tree_head.id = 0;
}
