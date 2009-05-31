// e3view superblock utilities
// Utility to make sense out of really damaged ext2/ext3 filesystems.
//
// If you have to make an assumption, write it down. Better assumptions may
// lead to better grades.

#include <linux/fs.h>
#include <linux/ext2_fs.h>
#include <stdio.h>

#include "e3tools.h"
#include "superblock.h"

void superblock_show(e3tools_t *e3t)
{
	printf("Superblock from block group %d\n", e3t->sb.s_block_group_nr);
	printf("\tMagic         : 0x%04X (%s)\n", e3t->sb.s_magic, (e3t->sb.s_magic == 0xEF53) ? "correct" : "INCORRECT");
	printf("\tInodes        : %d\n", e3t->sb.s_inodes_count);
	printf("\tBlocks        : %d\n", e3t->sb.s_blocks_count);
	printf("\tBlock size    : %d\n", 1024 << e3t->sb.s_log_block_size);
	printf("\t   on-disk    : %d\n", e3t->sb.s_log_block_size);
	printf("\tFragment size : %d\n", (e3t->sb.s_log_frag_size > 0) ? (1024 << e3t->sb.s_log_frag_size) : (1024 >> -e3t->sb.s_log_frag_size));
	printf("\t   on-disk    : %d\n", e3t->sb.s_log_frag_size);
	printf("\tJournal inode : %d\n", e3t->sb.s_journal_inum);
	printf("\tInode size    : %d\n", e3t->sb.s_inode_size);
	printf("\n");
	printf("Group information:\n");
	printf("\tFirst data block : %d\n", e3t->sb.s_first_data_block);
	printf("\tBlocks per       : %d\n", e3t->sb.s_blocks_per_group);
	printf("\tFragments per    : %d\n", e3t->sb.s_frags_per_group);
	printf("\tInodes per       : %d\n", e3t->sb.s_inodes_per_group);
	if (e3t->sb.s_feature_ro_compat & EXT2_FEATURE_RO_COMPAT_SPARSE_SUPER)
		printf("\tSuperblocks are sparse -- available at block groups 0, 1, powers of 3, powers of 5, powers of 7.\n");
	printf("\n");
	if (e3t->sb.s_log_block_size < 1)
	{
		printf("1k blocks are not supported! (Potentially bad superblock?)\n");
		return;
	}
	
	printf("OK, so expect:\n");
	printf("\tFull block groups      : %d\n", e3t->sb.s_blocks_count / e3t->sb.s_blocks_per_group);
	printf("\t   (Blocks left over?) : %d\n", e3t->sb.s_blocks_count % e3t->sb.s_blocks_per_group);
}
