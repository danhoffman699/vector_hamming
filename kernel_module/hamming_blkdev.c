#include "hamming.h"
#include "hamming_fast_logic.h"
#include "hamming_fast_logic_simple.h"

/**
 * \file hamming_blkdev.c
 * \brief Block IO initialization and callbacks
 */

/**
 * \brief Fill a read BIO request
 *
 * If there is a node in the tree at the address, copy the contents over
 * If there is no node in the tree at the address, blank the memory
 *
 * NOTE: we can specify DISCARD/WRITE_ZEROES, and truncate the tree for size,
 * but that also means we have to blank the memory if we don't have a node,
 * which isn't an expensive operation
 *
 * \param[in] sector		Sector to read
 * \param[in] page_ptr		Pointer to page, passed by Linux, we need to populate
 * \param[in] page_len		Length of page information we need to populate
 *
 * \return -EIO on errors, 0 otherwise
 */
static int hamming_bvec_read(u32 sector, u8 *page_ptr, u32 page_len){
	u8 *end_ptr;
	u8 *sector_ptr;

	end_ptr = page_ptr + page_len;
	while(page_ptr <= end_ptr - SECTOR_SIZE){
		sector_ptr = hamming_tree_sector_simple(SECTOR_TO_PAGE(sector), SECTOR_TO_CHUNK(sector), false);
		if(sector_ptr == NULL){
			memset(page_ptr, 0, SECTOR_SIZE);
		}else{
			memcpy(page_ptr, sector_ptr, SECTOR_SIZE);
		}
		page_ptr += SECTOR_SIZE;
		sector++;
		if(unlikely(sector > SECTOR_COUNT)){
			return -EIO;
		}
	}
	return 0;
}

/**
 * \brief Fill a write BIO request
 *
 * If there is a node in the tree at the address, copy the data over directly
 * If there is no node in the tree at the address, allocate and copy over
 *
 * See hamming_tree_sector_simple for allocation of new nodes
 *
 * \param[in] sector		Sector to write
 * \param[in] page_ptr		Pointer to page, passed by Linux, we need to write
 * \param[in] page_len		Length of page information we need to write
 * 
 * \return -EIO on errors, 0 otherwise
 */
static int hamming_bvec_write(u32 sector, u8 *page_ptr, u32 page_len){
	u8 *end_ptr;
	u8 *sector_ptr;
	end_ptr = page_ptr + page_len;

	while(page_ptr <= end_ptr - SECTOR_SIZE){
		sector_ptr = hamming_tree_sector_simple(SECTOR_TO_PAGE(sector), SECTOR_TO_CHUNK(sector), true);
		if(unlikely(sector_ptr == NULL)){
			printk(KERN_ERR "write operation failed\n");
			return -EIO; // realistically it fell out of bounds for the sector
		}
		memcpy(sector_ptr, page_ptr, SECTOR_SIZE);
		page_ptr += SECTOR_SIZE;
		sector++;
	}
	return 0;
}

/**
 * \brief Handle individual BIO requests
 *
 * Calls either hamming_bvec_write or hamming_bvec_read
 *
 * Since the memory is HIGHMEMORY, we have to kmap/kunmap pages to get a kernelspace
 * pointer
 *
 * \param[in] bvec		Vector of current BIO request, contains page information
 * \param[in] sector	Sector to operate on
 * \param[in] is_write	True for write operation, false for read
 */
static int hamming_bvec_rw(struct bio_vec *bvec, u32 sector, bool is_write){
	u32 ret;
	u8 *bv_page_ptr = kmap_atomic(bvec->bv_page);
	if(is_write){
		ret = hamming_bvec_write(sector, bv_page_ptr, bvec->bv_len);
	}else{
		ret = hamming_bvec_read(sector, bv_page_ptr, bvec->bv_len);
	}
	kunmap_atomic(bv_page_ptr);
	return ret;
}

/**
 * \brief Callback for BIO requests
 *
 * Traditionally, queues are used, since real hardware has unreliable latency.
 * Since we aren't, we use the callback interface for block requests, which is
 * faster and less error prone (also used by other virt interfaces).
 *
 * NOTE: return values here hint at what can be read next in the least amount
 * of time (IIRC), since there's no locality in access, we pass nothing
 *
 * \param[in] q			Request queue (currently unused)
 * \param[in] bio		BIO request chunk
 *
 * \return BLK_QC_T_NONE
 */
static blk_qc_t hamming_make_request(struct request_queue *q, struct bio *bio){
	struct bio_vec bvec;
	struct bvec_iter iter;
	int bio_ret;
	bool is_write;

	if(bio_op(bio) == REQ_OP_DISCARD ||
	   bio_op(bio) == REQ_OP_WRITE_ZEROES){
		// TODO: should we investigate REQ_OP_WRITE_ZEROES?
		bio_ret = 0;
	}else{
		u32 cur_sector;
		cur_sector = bio->bi_iter.bi_sector;

		is_write = op_is_write(bio_op(bio));
		bio_for_each_segment(bvec, bio, iter){
			bio_ret = hamming_bvec_rw(&bvec, cur_sector, is_write);
			if(unlikely(bio_ret < 0)){
				printk(KERN_ERR "hamming_rw_page failed with %d\n", bio_ret);
				goto out;
			}
			if(unlikely(bvec.bv_len % 512)){
				printk(KERN_ERR "we currently have no way of dealing with sub-sector precision, weird\n");
				goto out;
			}
			cur_sector += bvec.bv_len/512;
		}
		//printk(KERN_ERR "bio request %d at sector %ld sector count %ld\n", is_write, bio->bi_iter.bi_sector, cur_sector - bio->bi_iter.bi_sector);
	}
	bio_endio(bio);
	return BLK_QC_T_NONE;

out:
	bio_io_error(bio);
	return BLK_QC_T_NONE;
}

static int hamming_major; // block device

/**
 * \brief Handle ioctl requests
 *
 * Currently not used, but trying to fill out block_device_operations so we are
 * more flexible and presentable as a project
 *
 * \param[in] bdev		Block device
 * \param[in] mode		Permissions
 * \param[in] cmd		ioctl command
 * \param[in] arg		ioctl argument
 *
 * \return Negaitve on failure, 0 otherwise
 */

static int hamming_blkdev_ioctl(struct block_device *bdev, fmode_t mode, unsigned int cmd, unsigned long arg){
    return 0;
}

/**
 * \brief Handle new block device creation
 * 
 * \param[in] bdev		Block device
 * \param[in] mode		Permissions
 *
 * \return Negative on failure, 0 otherwise
 */

static int hamming_blkdev_open(struct block_device *bdev, fmode_t mode){
    return 0;
}

/**
 * \brief Handle new block device deletion
 *
 * \param[in] disk		Generated disk to destroy
 * \param[in] mode		Permissions
 *
 * \return Negative on failure, 0 otherwise
 */

static int hamming_blkdev_release(struct gendisk *disk, fmode_t mode){
    return 0;
}


/**
 * \brief Verify integrity of *all* data
 *
 * This is a fairly tall order
 *
 * \param[in] disk		Generated disk to validate
 *
 * \return Negative on failure, 0 otherwise
 */

static int hamming_blkdev_validate(struct gendisk *disk){
    return 0;
}

static const struct block_device_operations hamming_devops = {
    /* .open = hamming_blkdev_open, */
    /* .release = hamming_blkdev_release, */
    /* .ioctl = hamming_blkdev_ioctl, */
    /* .revalidate = hamming_blkdev_revalidate, */
	.owner = THIS_MODULE
};

/**
 * \brief Initialize the block layer and block device
 *
 * This generates one disk, /dev/hamming0, of a set size (currently 1GB),
 * as well as tunes and configures parameters for reasonable operation.
 *
 * We can add a few options to increase performance, but a lot of them are built
 * to optimize the binary tree model directly, and may not properly reflect real
 * hardware when we map this onto something physical, so I don't plan to seek
 * those out for now (DISCARD/WRITE_ZEROES namely).
 *
 * \return Negative value on error, 0 otherwise
 */
static int hamming_blkdev_init(void){
    hamming->frontend.mode = FRONT_BLOCK_IO;
    
	hamming_major = register_blkdev(0, "hamming");
	if (hamming_major <= 0) {
		pr_err("Unable to get major number\n");
		return -ENOMEM;
	}
	hamming->frontend.block_io.queue = blk_alloc_queue(GFP_KERNEL);
	if (hamming->frontend.block_io.queue == NULL) {
		pr_err("Couldn't allocate block device queue\n");
        return -EINVAL;
	}
	blk_queue_make_request(hamming->frontend.block_io.queue, hamming_make_request);

	hamming->frontend.block_io.disk = alloc_disk(1);
	if(!hamming->frontend.block_io.disk) {
		pr_err("Error allocating disk structure for device %d\n",
		       device_id);
        return -ENOMEM;
	}

	hamming->frontend.block_io.disk->major = hamming_major;
	hamming->frontend.block_io.disk->first_minor = device_id;
	hamming->frontend.block_io.disk->fops = &hamming_devops; // ?
    hamming->frontend.block_io.disk->queue = hamming->frontend.block_io.queue;
	hamming->frontend.block_io.disk->queue->queuedata = hamming;
	hamming->frontend.block_io.disk->private_data = hamming;
	snprintf(hamming->frontend.block_io.disk->disk_name, 16, "hamming%d", device_id);

	set_capacity(hamming->frontend.block_io.disk, SECTOR_COUNT); // This should be set through sysfs, but that's not a thing yet, only inline now for testing
	queue_flag_set_unlocked(QUEUE_FLAG_NONROT, hamming->frontend.block_io.disk->queue);
	queue_flag_clear_unlocked(QUEUE_FLAG_ADD_RANDOM, hamming->frontend.block_io.disk->queue);
	// Used to sanity check size of IO requests
	blk_queue_physical_block_size(hamming->frontend.block_io.disk->queue, PAGE_SIZE);
	blk_queue_logical_block_size(hamming->frontend.block_io.disk->queue, PAGE_SIZE);

	blk_queue_io_min(hamming->frontend.block_io.disk->queue, PAGE_SIZE);
	blk_queue_io_opt(hamming->frontend.block_io.disk->queue, PAGE_SIZE);

	// No way to specify read size, so we just have to roll with it

    hamming->frontend.block_io.disk->queue->limits.discard_granularity = PAGE_SIZE;
	blk_queue_max_discard_sectors(hamming->frontend.block_io.disk->queue, UINT_MAX);
	queue_flag_set_unlocked(QUEUE_FLAG_DISCARD, hamming->frontend.block_io.disk->queue);
	blk_queue_max_write_zeroes_sectors(hamming->frontend.block_io.disk->queue, UINT_MAX);

	printk(KERN_INFO "Adding single Hamming disk\n");
	add_disk(hamming->frontend.block_io.disk);
    return 0;
}

/**
 * \brief Close block device
 *
 * Unregisters the block device interface, doesn't touch the binary tree
 *
 * \return Negative on failure, 0 otherwise
 */
static int hamming_blkdev_close(void){
    if(hamming->frontend.mode == FRONT_BLOCK_IO){
        unregister_blkdev(hamming_major, "hamming");
        if(hamming){
            if(hamming->frontend.block_io.disk){
                del_gendisk(hamming->frontend.block_io.disk);
                put_disk(hamming->frontend.block_io.disk);
                hamming->frontend.block_io.disk = NULL;
            }
            if(hamming->frontend.block_io.queue){
                blk_cleanup_queue(hamming->frontend.block_io.queue);
                hamming->frontend.block_io.queue = NULL;
            }
        }
    }else pr_err("init/close mismatch with blkdev (?)\n");
    return 0;
}
