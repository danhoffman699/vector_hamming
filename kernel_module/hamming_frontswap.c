/**
 * \file hamming_frontswap.c
 * \brief Frontswap interface for RAM
 *
 * Frontswap is a non BIO based backing for swappable pages.
 *
 * Since frontswap can reject any and all pages, any page swapped into 
 * frontswap must also have a position in a real backing device. However,
 * if we can guarantee inside of the frontswap device that all pages
 * will be accepted, we can create and lie about the size of swap we
 * have, since no pages will be committed to it (frontswap always takes
 * precedence over traditional swap)
 */

#include <linux/frontswap.h>

/**
 * \brief swapon equivalent of frontswap
 *
 * Called when we are initialized with the frontswap system
 */
static void hamming_frontswap_op_init(unsigned id){
    hamming->frontend.mode = FRONT_FRONTSWAP;
    hamming->frontend.frontswap.swap_id = id;
};

/**
 * \brief Commit pages to frontswap device
 */
static int hamming_frontswap_op_store(unsigned id, pgoff_t offset, struct page *page){
    return 0;
}

/**
 * \brief Load page from frontswap device
 */
static int hamming_frontswap_op_load(unsigned id, pgoff_t offset, struct page *page){
    return 0;
}

/**
 * \brief Invalidate page
 */
static void hamming_frontswap_op_invalidate_page(unsigned id, pgoff_t offset){
}

/**
 * \brief Invalidate area
 */
static void hamming_frontswap_op_invalidate_area(unsigned id){
}

/**
 * \breif Attempt to write pages to frontswap
 *
 * It writes to whatever the backend is, regardless
 */

static struct frontswap_ops hamming_frontswap_ops = {
    .init = hamming_frontswap_op_init,
    .store = hamming_frontswap_op_store,
    .load = hamming_frontswap_op_load,
    .invalidate_page = hamming_frontswap_op_invalidate_page,
    .invalidate_area = hamming_frontswap_op_invalidate_area
};

/**
 * \brief Initialize frontswap
 *
 * Register this device through frontswap, and create a fake block device
 * with some ridiculously large size o
 */
static int hamming_frontswap_init(void){
    hamming->frontend.mode = FRONT_FRONTSWAP;
    frontswap_register_ops(&hamming_frontswap_ops);
    frontswap_writethrough(false);
    return 0;
}

static int hamming_frontswap_close(void){
    return 0;
}
