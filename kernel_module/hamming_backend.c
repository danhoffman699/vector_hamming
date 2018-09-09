/**
 * \file hamming_backend.c
 * \brief Helper functions for backend access
 *
 * Since we never really care what the underlying implementation is,
 * we just generically read to and write from addresses here
 *
 * Actual error correction, reporting, logging, will probably end up here as well
 */

/**
 * \brief Commit data to backing device
 *
 * Either write to binary tree, or forward as a BIO request to some other medium
 *
 * \param[in] hamming		Hamming device we are operating on
 * \param[in] offset		Offset in medium to write to (probably sector)
 * \param[in] data			Data to commit to medium
 * \param[in] len			Length of data to commit to medium
 *
 * \return Negative on failure, 0 otherwise
 */
static int hamming_write(hamming_t *hamming, u64 offset, u8 *data, size_t len){
    VM_BUG_ON(hamming != NULL);
    VM_BUG_ON(data != NULL);
    int i;
    
    switch(hamming->backend.mode){
    case BACK_BIN_TREE:
        if(len % 512){
            pr_err("Length required to write not a multiple of SECTOR_SIZE, not writing");
            return -EINVAL;
        }
        for(i = 0;i < len / SECTOR_SIZE;i++){
            void *sector = hamming_tree_sector_simple(SECTOR_TO_PAGE(offset),
                                                      SECTOR_TO_CHUNK(offset),
                                                      true);
            if(sector == NULL){
                pr_err("No valid sector found for write\n");
                return -EIO;
            }
            memcpy(sector, data + SECTOR_SIZE*i, SECTOR_SIZE);
        }
        break;
    case BACK_BLOCK_IO:
        return -EINVAL;
        break;
    default:
        BUG();
        break;
    }
    return 0;
}

/**
 * \brief Read data from backing device
 *
 * I'm assuming if a request falls outside the medium, returning zeroes is
 * a valid response, so there's never a situation where len isn't "read" in.
 *
 * \param[in] hamming		Hamming device to operate on
 * \param[in] offset		Offset in medium to read from to (probably sector)
 * \param[in] data			Data to read back
 * \param[in] len			Length of data to commit to medium
 *
 * \return Negative on failure, 0 otherwise
 */
static int hamming_read(hamming_t *hamming, u64 offset, u8 *data, size_t len){
    VM_BUG_ON(hamming != NULL);
    VM_BUG_ON(data != NULL);
    int i;

    switch(hamming->backend.mode){
    case BACK_BIN_TREE:
        if(len % 512){
            pr_err("Length required to write not a multiple of SECTOR_SIZE, not writing");
            return -EINVAL;
        }
        for(i = 0;i < len / SECTOR_SIZE;i++){
            void *sector = hamming_tree_sector_simple(SECTOR_TO_PAGE(offset),
                                                      SECTOR_TO_CHUNK(offset),
                                                      false);
            if(sector == NULL){
                memset(data + SECTOR_SIZE*i, 0, SECTOR_SIZE);
            }else{
                memcpy(data + SECTOR_SIZE*i, sector, SECTOR_SIZE);
            }
        }
        break;
    case BACK_BLOCK_IO:
        return -EINVAL;
        break;
    default:
        BUG();
    }
    return 0;
}
