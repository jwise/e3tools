// e3view superblock utilities
// Utility to make sense out of really damaged ext2/ext3 filesystems.
//
// If you have to make an assumption, write it down. Better assumptions may
// lead to better grades.

#include <linux/fs.h>
#include <linux/ext2_fs.h>
#include <stdio.h>

#include "superblock.h"

void superblock_show(struct ext2_super_block *sb)
{
	printf("Superblock from block group %d\n", sb->s_block_group_nr);
	printf("\tMagic         : 0x%04X (%s)\n", sb->s_magic, (sb->s_magic == 0xEF53) ? "correct" : "INCORRECT");
	printf("\tInodes        : %d\n", sb->s_inodes_count);
	printf("\tBlocks        : %d\n", sb->s_blocks_count);
	printf("\tBlock size    : %d\n", 1024 << sb->s_log_block_size);
	printf("\t   on-disk    : %d\n", sb->s_log_block_size);
	printf("\tFragment size : %d\n", (sb->s_log_frag_size > 0) ? (1024 << sb->s_log_frag_size) : (1024 >> -sb->s_log_frag_size));
	printf("\t   on-disk    : %d\n", sb->s_log_frag_size);
	printf("\tJournal inode : %d\n", sb->s_journal_inum);
	printf("\tInode size    : %d\n", sb->s_inode_size);
	printf("\n");
	printf("Group information:\n");
	printf("\tFirst data block : %d\n", sb->s_first_data_block);
	printf("\tBlocks per       : %d\n", sb->s_blocks_per_group);
	printf("\tFragments per    : %d\n", sb->s_frags_per_group);
	printf("\tInodes per       : %d\n", sb->s_inodes_per_group);
	if (sb->s_feature_ro_compat & EXT2_FEATURE_RO_COMPAT_SPARSE_SUPER)
		printf("\tSuperblocks are SPARSE! Available at block groups 0, 1, powers of 3, powers of 5, powers of 7.\n");
	printf("\n");
	if (sb->s_log_block_size < 1)
	{
		printf("1k blocks are not supported! (Potentially bad superblock?)\n");
		return;
	}
	
	printf("OK, so expect:\n");
	printf("\tBlock groups         : %d\n", sb->s_blocks_count / sb->s_blocks_per_group);
	printf("\t   (Blocks left over?) : %d\n", sb->s_blocks_count % sb->s_blocks_per_group);
}
