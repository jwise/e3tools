// e3view
// Utility to make sense out of really damaged ext2/ext3 filesystems.
//
// If you have to make an assumption, write it down. Better assumptions may
// lead to better grades.
//
// Useful resources:
//  * http://www.nongnu.org/ext2-doc/ext2.html

#define _LARGEFILE64_SOURCE
#include <linux/fs.h>
#include <linux/ext2_fs.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdint.h>
#include <unistd.h>
#include <stdio.h>

#include "diskio.h"

int ispow(int n, int p)
{
	if (n == 1)
		return 1;
	if (n % p)
		return 0;
	return ispow(n / p, p);
}

int sb_blocks(struct ext2_super_block *sb)
{
	int bgs = sb->s_blocks_count / sb->s_blocks_per_group;
	if (sb->s_blocks_count % sb->s_blocks_per_group)
		bgs++;
	int bytes_per_block = 1024 << sb->s_log_block_size;
	int bgblocks = sizeof(struct ext2_group_desc) * (bgs) / bytes_per_block;
	if (sizeof(struct ext2_group_desc) * (bgs) % bytes_per_block)
		bgblocks++;
	return 1 /* Superblock */ + /*bgblocks*/ 0x400 /* wtf? */;
}

int block_group_has_sb(struct ext2_super_block *sb, int bg)
{
	if (!(sb->s_feature_ro_compat & EXT2_FEATURE_RO_COMPAT_SPARSE_SUPER))
		return 1;
	return (bg == 0) || (bg == 1) || ispow(bg, 3) || ispow(bg, 5) || ispow(bg, 7);
}

int block_is_in_block_group(struct ext2_super_block *sb, int block, int bg)
{
	return (block / sb->s_blocks_per_group) == bg;
}

int block_group_expected_block_bitmap(struct ext2_super_block *sb, int bg)
{
	return bg * sb->s_blocks_per_group + (block_group_has_sb(sb, bg) ? sb_blocks(sb) : 0);
}

void show_block_group_desc_table(struct ext2_super_block *sb)
{
	int bgs = sb->s_blocks_count / sb->s_blocks_per_group;
	int bytes_per_block = 1024 << sb->s_log_block_size;
	int sectors_per_block = (1024 / BYTES_PER_SECTOR) << sb->s_log_block_size;
	int curbg;
	sector_t sector;
	struct ext2_group_desc sect[BYTES_PER_SECTOR / sizeof(struct ext2_group_desc)];
	int dirty = 0;
	
	printf("Block descriptor table from block group %d\n", sb->s_block_group_nr);
	printf("Starts on block %d\n", sb->s_block_group_nr * sb->s_blocks_per_group + 1);
	sector = (sb->s_block_group_nr * sb->s_blocks_per_group + 1LL) * (sector_t)sectors_per_block;
	printf("  ... or sector %d\n", sector);
	printf("Expecting %d block groups\n", bgs);
	printf("Expecting %d bytes of block group\n", sizeof(struct ext2_group_desc) * (bgs));
	printf("Expecting %d sectors of block group\n", sizeof(struct ext2_group_desc) * (bgs) / BYTES_PER_SECTOR);
	printf("Expecting %d blocks of block group\n", sizeof(struct ext2_group_desc) * (bgs) / bytes_per_block);
	printf("OK, let's do this!\n");
	printf("\n");
	
	for (curbg = 0; curbg < bgs; curbg++)
	{
		int pos = curbg % (BYTES_PER_SECTOR / sizeof(struct ext2_group_desc));
		if (pos == 0)
		{
			if (dirty)
			{
				sector--;
				printf("Writing back changed sector: %lld\n", sector);
				if (disk_write_sector(sector, (uint8_t *)sect) < 0)
				{
					fflush(stdout);
					perror("write_sector");
					return;
				}
				sector++;
				dirty = 0;
			}
			printf("Reading from new sector: %lld\n", sector);
			if (disk_read_sector(sector, (uint8_t *)sect) < 0)
			{
				fflush(stdout);
				perror("read_sector");
				return;
			}
			sector++;
		}
		printf("\tBlock group %d\n", curbg);
		if (block_group_has_sb(sb, curbg))
			printf("\t\tHas superblock\n");
		printf("\t\tBitmap block : %12d (0x%08x)\n", sect[pos].bg_block_bitmap, sect[pos].bg_block_bitmap);
		if (!block_is_in_block_group(sb, sect[pos].bg_block_bitmap, curbg))
			printf("\t\t               ...looks bad!\n");
		if (sect[pos].bg_block_bitmap != block_group_expected_block_bitmap(sb, curbg))
		{
			printf("\t\t               ...but expected %08x! Fixing...\n", block_group_expected_block_bitmap(sb, curbg));
			sect[pos].bg_block_bitmap = block_group_expected_block_bitmap(sb, curbg);
			dirty++;
		}
		printf("\t\tInode block  : %12d (0x%08x)\n", sect[pos].bg_inode_bitmap, sect[pos].bg_inode_bitmap);
		if (!block_is_in_block_group(sb, sect[pos].bg_inode_bitmap, curbg))
			printf("\t\t               ...looks bad!\n");
		if (sect[pos].bg_inode_bitmap != (block_group_expected_block_bitmap(sb, curbg) + 1))
		{
			printf("\t\t               ...but expected %08x! Fixing...\n", block_group_expected_block_bitmap(sb, curbg) + 1);
			sect[pos].bg_inode_bitmap = block_group_expected_block_bitmap(sb, curbg) + 1;
			dirty++;
		}
		printf("\t\tInode table  : %12d (0x%08x)\n", sect[pos].bg_inode_table, sect[pos].bg_inode_table);
		if (!block_is_in_block_group(sb, sect[pos].bg_inode_table, curbg))
			printf("\t\t               ...looks bad!\n");
		if (sect[pos].bg_inode_table != (block_group_expected_block_bitmap(sb, curbg) + 2))
		{
			printf("\t\t               ...but expected %08x! Fixing...\n", block_group_expected_block_bitmap(sb, curbg) + 2);
			sect[pos].bg_inode_table = block_group_expected_block_bitmap(sb, curbg) + 2;
			dirty++;
		}
	}
}

void show_superblock(sector_t s)
{
	struct ext2_super_block sb;
	
	if (disk_read_sector(s, (uint8_t*)&sb) < 0)
	{
		perror("read_sector");
		return;
	}
	
	printf("Superblock from sector %d\n", s);
	printf("\tMagic         : 0x%04X (%s)\n", sb.s_magic, (sb.s_magic == 0xEF53) ? "correct" : "INCORRECT");
	printf("\tInodes        : %d\n", sb.s_inodes_count);
	printf("\tBlocks        : %d\n", sb.s_blocks_count);
	printf("\tBlock size    : %d\n", 1024 << sb.s_log_block_size);
	printf("\t   on-disk    : %d\n", sb.s_log_block_size);
	printf("\tFragment size : %d\n", (sb.s_log_frag_size > 0) ? (1024 << sb.s_log_block_size) : (1024 >> -sb.s_log_block_size));
	printf("\t   on-disk    : %d\n", sb.s_log_frag_size);
	printf("\tJournal inode : %d\n", sb.s_journal_inum);
	printf("\tThis SB from  : %d\n", sb.s_block_group_nr);
	printf("\n");
	printf("Group information:\n");
	printf("\tFirst data block : %d\n", sb.s_first_data_block);
	printf("\tBlocks per       : %d\n", sb.s_blocks_per_group);
	printf("\tFragments per    : %d\n", sb.s_frags_per_group);
	printf("\tInodes per       : %d\n", sb.s_inodes_per_group);
	if (sb.s_feature_ro_compat & EXT2_FEATURE_RO_COMPAT_SPARSE_SUPER)
		printf("\tSuperblocks are SPARSE! Available at block groups 0, 1, powers of 3, powers of 5, powers of 7.\n");
	printf("\n");
	if (sb.s_log_block_size < 1)
	{
		printf("1k blocks are not supported! (Potentially bad superblock?)\n");
		return;
	}
	
	printf("OK, so expect:\n");
	printf("\tBlock groups         : %d\n", sb.s_blocks_count / sb.s_blocks_per_group);
	printf("\t   (Blocks left over?) : %d\n", sb.s_blocks_count % sb.s_blocks_per_group);
	printf("\n");
	
	show_block_group_desc_table(&sb);
}

int main()
{
	printf("Trying to read superblock...\n");
	show_superblock(2);
	return 0;
}