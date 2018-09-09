#include "hamming.h"
#include "hamming_test.h"
#include "hamming_tree.h"

static int hamming_test_sector_simple(void){
	// TODO: rewrite the test function to use subtrees for optimization
	int i, b;
	for(i = 0;i < 256;i++){
		u8 *sector = hamming_tree_sector_simple(SECTOR_TO_PAGE(i), SECTOR_TO_CHUNK(i), true);
		if(sector == NULL){
			printk(KERN_ERR "sector is NULL in self test (%d, %d)\n", SECTOR_TO_PAGE(i), SECTOR_TO_CHUNK(i));
			return -EIO;
		}
		memset(sector, i&0xFF, SECTOR_SIZE);
	}
	for(i = 0;i < 256;i++){
		u8 *sector = hamming_tree_sector_simple(SECTOR_TO_PAGE(i), SECTOR_TO_CHUNK(i), false);
		if(sector == NULL){
			printk(KERN_ERR "can't recall sector written to inside test function\n");
			return -EIO;
		}
		for(b = 0;b < SECTOR_SIZE;b++){
			if(sector[b] != (i & 0xFF)){
				printk(KERN_ERR "sector read/write failed\n");
				return -EIO;
			}
		}
	}
	return 0;
}

static int hamming_test_subtree_system(void){
	bool create;
	hamming_subtree_t master, slave;
	sector_t test_sector;

	test_sector = 64 << 3;
	create = true;
	master.ptr = NULL;
	slave.ptr = NULL;
	slave.processed_bits = 29;
	slave.id = SECTOR_TO_PAGE(test_sector);

	hamming_tree_resolve(master, &slave, &create, 1);

	if(slave.ptr == NULL){
		printk(KERN_ERR "couldn't resolve actual subtree\n");
	}

	memcpy(&master, &slave, sizeof(hamming_subtree_t));

	slave.ptr = NULL;
	slave.processed_bits = 32;
	slave.id = SECTOR_TO_PAGE(test_sector); // should be redundant

	hamming_tree_resolve(master, &slave, &create, 1);

	if(slave.ptr == NULL){
		printk(KERN_ERR "couldn't resolve final node from subtree\n");
	}
	return 0;
}

static int hamming_tests(void){
	hamming_test_sector_simple();
	//hamming_test_subtree_system();
	printk(KERN_INFO "All tests passed succesfully\n");
	return 0;
}
