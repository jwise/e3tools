#ifndef _SUPERBLOCK_H
#define _SUPERBLOCK_H

#include <linux/fs.h>
#include <linux/ext2_fs.h>

#include "e3tools.h"

#define SB_BLOCK_SIZE(sb) (1024 << (sb)->s_log_block_size)
#define SB_GROUPS(sb) ((sb)->s_blocks_count / (sb)->s_blocks_per_group + !!((sb)->s_blocks_count % (sb)->s_blocks_per_group))
extern void superblock_show(e3tools_t *sb);

#endif
