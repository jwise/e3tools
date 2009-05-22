#ifndef _BLOCKGROUP_H
#define _BLOCKGROUP_H

#include <stdint.h>

typedef uint32_t block_t;

extern void block_group_desc_table_show(struct ext2_super_block *sb);
extern void block_group_desc_table_repair(struct ext2_super_block *sb);
extern block_t block_group_inode_table_block(struct ext2_super_block *sb, int bg);

#endif
