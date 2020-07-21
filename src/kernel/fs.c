/* Copyright (C) 2013 by John Cronin <jncronin@tysos.org>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:

 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.

 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include <stdint.h>
#include <kernel/block.h>
#include <kernel/vfs.h>
#include <kernel/mem.h>
#include <common/stdlib.h>

// Features
#define ENABLE_SD 1
#define ENABLE_FAT 1

// Space for 32 x 512 byte cache areas
#define BLOCK_CACHE_SIZE	0x4000

#ifdef ENABLE_SD
int sd_card_init(struct block_device **dev);
#endif
#ifdef ENABLE_MBR
int read_mbr(struct block_device *, struct block_device ***, int *);
#endif
#ifdef ENABLE_FAT
int fat_init(struct block_device *, struct fs **);
#endif

struct block_device * libfs_init() {
    struct block_device *sd_dev = NULL;
	sd_card_init(&sd_dev);
    return sd_dev;
}

int register_fs(struct block_device *dev, int part_id) {
    switch(part_id) {
        case 0:
        case 1:
        case 4:
        case 6:
        case 0xb:
        case 0xc:
        case 0xe:
        case 0x11:
        case 0x14:
        case 0x1b:
        case 0x1c:
        case 0x1e:
            fat_init(dev, &dev->fs);
            break;
    }

    if(dev->fs) {
        vfs_register(dev->fs);
        return 0;
    }
    else
        return -1;
}

int fs_interpret_mode(const char *mode) {
    // Interpret mode arguments
    if(!strcmp(mode, "r"))
        return VFS_MODE_R;
    if(!strcmp(mode, "r+"))
        return VFS_MODE_RW;
    if(!strcmp(mode, "w"))
        return VFS_MODE_W | VFS_MODE_CREATE;
    if(!strcmp(mode, "w+"))
        return VFS_MODE_RW | VFS_MODE_CREATE;
    if(!strcmp(mode, "a"))
        return VFS_MODE_W | VFS_MODE_APPEND | VFS_MODE_CREATE;
    if(!strcmp(mode, "a+"))
        return VFS_MODE_RW | VFS_MODE_APPEND | VFS_MODE_CREATE;
    return 0;
}

/* The fread/fwrite() functions in filesystems code shares a lot of common functionality
 * We provide that here
 * There are essentially two types of filesystem as regards to indexing blocks
 * within a file.
 * Assume a file contains n blocks and we want block i.
 * Filesystems like ext2, nofs can tell us the block number from i
 * Ones like FAT need to know the block number i - 1 and work it out from there
 *
 * Thus, if the block number can be calculated from i, can_index_blocks is set
 * to 1.
 *
 * fs_fread fills in as many of the parameters of get_next_block_num as it can
 */

uint64_t fs_fread(uint32_t (*get_next_bdev_block_num)(uint32_t f_block_idx, FILE *s, void *opaque, int add_blocks),
                struct fs *fs, void *ptr, uint64_t byte_size, FILE *stream, void *opaque) {
    uint32_t fs_block_size = fs->block_size;

    // Determine first and last block indices within file
    uint32_t first_f_block_idx = div(stream->pos, fs_block_size);
    uint32_t first_f_block_offset = divmod(stream->pos, fs_block_size).mod;
    uint32_t last_pos = stream->pos + byte_size;
    uint32_t last_f_block_idx = div(last_pos, fs_block_size);
    uint32_t last_f_block_offset = divmod(last_pos, fs_block_size).mod;

    // Now iterate through the blocks
    uint32_t cur_block = first_f_block_idx;
    uint8_t *save_buf = (uint8_t *)ptr;
    int total_bytes_read = 0;
    while(cur_block <= last_f_block_idx) {
        uint32_t start_block_offset = 0;
        uint32_t last_block_offset = fs_block_size;

        // If we're the first block, adjust start_block_idx appropriately
        if(cur_block == first_f_block_idx)
            start_block_offset = first_f_block_offset;
        // If we're the last block, adjust last_block_idx appropriately
        if(cur_block == last_f_block_idx)
            last_block_offset = last_f_block_offset;

        uint32_t block_segment_length = last_block_offset - start_block_offset;

        // Get the filesystem block number
        uint32_t cur_bdev_block = get_next_bdev_block_num(cur_block, stream, opaque, 0);
        if(cur_bdev_block == 0xffffffff)
            return total_bytes_read;

        // If we can load an entire block, load it directly, else we have
        //  to load to a buffer somewhere and copy appropriately
        if((start_block_offset == 0) && (block_segment_length == fs_block_size)) {
            int bytes_read = block_read(fs->parent, save_buf, fs_block_size, cur_bdev_block);
            total_bytes_read += bytes_read;
            stream->pos += bytes_read;
            save_buf += bytes_read;
            if((uint32_t)bytes_read != fs_block_size)
                return total_bytes_read;
        } else {
            // We have to load to a temporary buffer
            uint8_t *temp_buf = (uint8_t *)kmalloc(fs_block_size);
            int bytes_read = block_read(fs->parent, temp_buf, fs_block_size, cur_bdev_block);
            if(last_block_offset > (uint32_t)bytes_read)
                last_block_offset = bytes_read;
            if(last_block_offset < start_block_offset)
                block_segment_length = 0;
            else
                block_segment_length = last_block_offset - start_block_offset;

            // Copy from the temporary buffer to the save buffer
            memcpy(save_buf, &temp_buf[start_block_offset], block_segment_length);

            // Increment the pointers
            total_bytes_read += block_segment_length;
            stream->pos += block_segment_length;
            save_buf += block_segment_length;

            kfree(temp_buf);

            if((uint32_t)bytes_read != fs_block_size)
                return total_bytes_read;
        }
        cur_block++;
    }
    return total_bytes_read;
}

uint64_t fs_fwrite(uint32_t (*get_next_bdev_block_num)(uint32_t f_block_idx, FILE *s, void *opaque, int add_blocks),
                 struct fs *fs, void *ptr, uint64_t byte_size, FILE *stream, void *opaque) {
    uint32_t fs_block_size = fs->block_size;

    // Files opened in mode "a+" always set the stream position to the end of the file before writing
    if((stream->mode & VFS_MODE_APPEND) && (stream->mode & VFS_MODE_R))
        stream->pos = stream->len;

    // Determine first and last block indices within file
    uint32_t first_f_block_idx = div(stream->pos, fs_block_size);
    uint32_t first_f_block_offset = divmod(stream->pos, fs_block_size).mod;
    uint32_t last_pos = stream->pos + byte_size;
    uint32_t last_f_block_idx = div(last_pos, fs_block_size);
    uint32_t last_f_block_offset = divmod(last_pos, fs_block_size).mod;

    // Now iterate through the blocks
    uint32_t cur_block = first_f_block_idx;
    uint8_t *save_buf = (uint8_t *)ptr;
    int total_bytes_written = 0;

    while(cur_block <= last_f_block_idx) {
        uint32_t start_block_offset = 0;
        uint32_t last_block_offset = fs_block_size;

        // If we're the first block, adjust start_block_idx appropriately
        if(cur_block == first_f_block_idx)
            start_block_offset = first_f_block_offset;
        // If we're the last block, adjust last_block_idx appropriately
        if(cur_block == last_f_block_idx)
            last_block_offset = last_f_block_offset;

        uint32_t block_segment_length = last_block_offset - start_block_offset;

        // Get the filesystem block number
        uint32_t cur_bdev_block = get_next_bdev_block_num(cur_block, stream, opaque, 1);
        if(cur_bdev_block == 0xffffffff)
            return total_bytes_written;

        // If we can save an entire block, save it directly, else we have
        //  to load to a buffer somewhere, edit, and save
        if((start_block_offset == 0) && (block_segment_length == fs_block_size)) {
            uint64_t bytes_written = block_write(fs->parent, save_buf, fs_block_size, cur_bdev_block);
            total_bytes_written += bytes_written;
            stream->pos += bytes_written;
            if(stream->pos > stream->len)
                stream->len = stream->pos;
            save_buf += bytes_written;
            if(bytes_written != fs_block_size)
                return total_bytes_written;
        } else {
            // We have to load to a temporary buffer
            uint8_t *temp_buf = (uint8_t *)kmalloc(fs_block_size);
            uint64_t bytes_read = block_read(fs->parent, temp_buf, fs_block_size, cur_bdev_block);
            if(bytes_read != fs_block_size)
                return total_bytes_written;

            // Edit the buffer
            memcpy(&temp_buf[start_block_offset], save_buf, block_segment_length);

            // Save the buffer
            uint64_t bytes_written = block_write(fs->parent, temp_buf, fs_block_size, cur_bdev_block);

            if(last_block_offset > bytes_written)
                last_block_offset = bytes_written;
            if(last_block_offset < start_block_offset)
                block_segment_length = 0;
            else
                block_segment_length = last_block_offset - start_block_offset;

            // Increment the pointers
            total_bytes_written += block_segment_length;
            stream->pos += block_segment_length;
            if(stream->pos > stream->len)
                stream->len = stream->pos;
            save_buf += block_segment_length;

            kfree(temp_buf);

            if(bytes_written != fs_block_size)
                return total_bytes_written;
        }
        cur_block++;
    }
    return total_bytes_written;
}