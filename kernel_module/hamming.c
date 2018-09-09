#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/bio.h>
#include <linux/bitops.h>
#include <linux/blkdev.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/sysfs.h>

/**
 * \file hamming.c
 * \brief Module registration, initialization, kernel hooks, etc
 */

#include "hamming.h"
#include "hamming_tree.h"
#include "hamming_test.h"

// logic from test program (only different enough to compile, printf->printk and smalls)
#include "hamming_fast_logic.c"
#include "hamming_fast_logic_simple.c"

static DEFINE_IDR(hamming_index_idr);
static DEFINE_MUTEX(hamming_index_mutex);
static hamming_t *hamming;
static int device_id; // idr

static struct attribute *hamming_control_class_attrs[] = {
	NULL
};

ATTRIBUTE_GROUPS(hamming_control_class);

static struct class hamming_control_class = {
	.name = "hamming-control",
	.owner = THIS_MODULE,
	.class_groups = hamming_control_class_groups
};

#include "hamming_blkdev.c"
#include "hamming_tree.c"
#include "hamming_test.c"
#include "hamming_frontswap.c"
#include "hamming_sysfs.c"

static int deinitialize(void){
	class_unregister(&hamming_control_class);
    idr_remove(&hamming_index_idr, device_id);
    hamming_blkdev_close();
    hamming_sysfs_close_error();
	if(hamming) { // semaphore lock is out of the scope of this function
		kfree(hamming);
		hamming = NULL;
	}
	return 0;
}

// TODO: clean up the hamming_init function a lot

/**
 * \brief Initializing hamming module
 *
 * Responsible for registering class IDs with the kernel, and calling the proper
 * subsystems we are configured to use, and perform some relevant self tests.
 *
 * Currently this can only be built with block IO and binary tree, but will be
 * expanded out to include
 *  - frontswap and binary tree
 *  - block IO and device mapper
 *
 * Honestly block IO and device mapper looks the most promising, since that's
 * the most useful for securing large data storage against insane SEUs.
 *
 * NOTE: does our MitySOM have load balancing on eMMC? That would be something
 * pretty useful to have here as well, but won't happen for practical reasons.
 */
static int __init hamming_init(void)
{
	int ret;
	ret = class_register(&hamming_control_class);
	if (ret) {
		pr_err("Unable to register hamming-control class\n");
		return -ENOMEM;
	}

	hamming = kzalloc(sizeof(hamming_t), GFP_KERNEL);
	if (!hamming) {
		pr_err("Unable to allocate hamming device\n");
		class_unregister(&hamming_control_class);
		return -ENOMEM;
	}

	// we are initialized at this point
	ret = idr_alloc(&hamming_index_idr, hamming, 0, 0, GFP_KERNEL);
	if (ret < 0) {
		pr_err("Couldn't get ID\n");
		deinitialize();
		return -EINVAL;
	}
	device_id = ret;

	init_rwsem(&hamming->lock);

    if(hamming_blkdev_init() < 0){
        printk(KERN_ERR "Can't initialize block device");
        return -EIO;
    }
	printk(KERN_INFO "Initializing tree structure\n");
	hamming_tree_init();
	if(hamming_tests() != 0){
		printk(KERN_ERR "hamming_self_test failed\n");
		deinitialize();
		return -EINVAL;
	}
	printk(KERN_INFO "Loaded ECC memory-based block device\n");

    // TODO: actually use frontswap

    if(hamming_sysfs_init_error() < 0){
        pr_err("sysfs initialization failed\n");
        return -EIO; // what are some better one s
    }
	return 0;
}

static void __exit hamming_exit(void)
{
	deinitialize();
	printk(KERN_INFO "Unloaded ECC memory-based block device\n");
}

module_init(hamming_init);
module_exit(hamming_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Daniel Hoffman");
MODULE_DESCRIPTION("ECC memory-based block device");
