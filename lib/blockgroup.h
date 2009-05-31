#ifndef _BLOCKGROUP_H
#define _BLOCKGROUP_H

#include <stdint.h>

#include "e3tools.h"

typedef uint32_t block_t;

extern void block_group_desc_table_show(e3tools_t *sb);
extern void block_group_desc_table_repair(e3tools_t *sb);
extern block_t block_group_inode_table_block(e3tools_t *sb, int bg);

#endif
