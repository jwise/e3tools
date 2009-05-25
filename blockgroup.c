// e3view block group / block group descriptor utilities
// Utility to make sense out of really damaged ext2/ext3 filesystems.
//
// If you have to make an assumption, write it down. Better assumptions may
// lead to better grades.

#include <linux/fs.h>
#include <linux/ext2_fs.h>
#include <stdio.h>
#include <stdint.h>

#include "e3bits.h"
#include "blockgroup.h"
#include "diskio.h"

block_t block_group_inode_table_block(struct ext2_super_block *sb, int bg)
{
	int sectors_per_block = (1024 / BYTES_PER_SECTOR) << sb->s_log_block_size;
	sector_t sector = (sb->s_block_group_nr * sb->s_blocks_per_group + 1LL) * (sector_t)sectors_per_block;
	struct ext2_group_desc sect[BYTES_PER_SECTOR / sizeof(struct ext2_group_desc)];
	
	sector += (bg * sizeof(struct ext2_group_desc)) / BYTES_PER_SECTOR;
	
	if (disk_read_sector(sector, (uint8_t *)sect) < 0)
	{
		fflush(stdout);
		perror("read_sector");
		return;
	}
	
	return sect[bg % (BYTES_PER_SECTOR / sizeof(struct ext2_group_desc))].bg_inode_table;
}

void block_group_desc_table_show(struct ext2_super_block *sb)
{
	int bgs = sb->s_blocks_count / sb->s_blocks_per_group;
	int bytes_per_block = 1024 << sb->s_log_block_size;
	int sectors_per_block = (1024 / BYTES_PER_SECTOR) << sb->s_log_block_size;
	int curbg;
	sector_t sector;
	struct ext2_group_desc sect[BYTES_PER_SECTOR / sizeof(struct ext2_group_desc)];
	
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
		if (e3_block_group_has_sb(sb, curbg))
			printf("\t\tHas superblock\n");

		printf("\t\tBitmap block : %12d (0x%08x)\n", sect[pos].bg_block_bitmap, sect[pos].bg_block_bitmap);
		if (!e3_block_is_in_block_group(sb, sect[pos].bg_block_bitmap, curbg))
			printf("\t\t               ...looks bad!\n");
		if (sect[pos].bg_block_bitmap != e3_block_group_expected_block_bitmap(sb, curbg))
			printf("\t\t               ...but expected %08x!\n", e3_block_group_expected_block_bitmap(sb, curbg));

		printf("\t\tInode block  : %12d (0x%08x)\n", sect[pos].bg_inode_bitmap, sect[pos].bg_inode_bitmap);
		if (!e3_block_is_in_block_group(sb, sect[pos].bg_inode_bitmap, curbg))
			printf("\t\t               ...looks bad!\n");
		if (sect[pos].bg_inode_bitmap != (e3_block_group_expected_block_bitmap(sb, curbg) + 1))
			printf("\t\t               ...but expected %08x!\n", e3_block_group_expected_block_bitmap(sb, curbg) + 1);

		printf("\t\tInode table  : %12d (0x%08x)\n", sect[pos].bg_inode_table, sect[pos].bg_inode_table);
		if (!e3_block_is_in_block_group(sb, sect[pos].bg_inode_table, curbg))
			printf("\t\t               ...looks bad!\n");
		if (sect[pos].bg_inode_table != (e3_block_group_expected_block_bitmap(sb, curbg) + 2))
			printf("\t\t               ...but expected %08x!\n", e3_block_group_expected_block_bitmap(sb, curbg) + 2);
	}
}

void block_group_desc_table_repair(struct ext2_super_block *sb)
{
	int bgs = sb->s_blocks_count / sb->s_blocks_per_group;
	int bytes_per_block = 1024 << sb->s_log_block_size;
	int sectors_per_block = (1024 / BYTES_PER_SECTOR) << sb->s_log_block_size;
	int curbg;
	sector_t sector;
	struct ext2_group_desc sect[BYTES_PER_SECTOR / sizeof(struct ext2_group_desc)];
	int dirty = 0;
	
	sector = (sb->s_block_group_nr * sb->s_blocks_per_group + 1LL) * (sector_t)sectors_per_block;
	for (curbg = 0; curbg < bgs; curbg++)
	{
		int pos = curbg % (BYTES_PER_SECTOR / sizeof(struct ext2_group_desc));
		if (pos == 0)
		{
			if (dirty)
			{
				sector--;
				printf("Writing back to repaired sector: %lld (%d changes)\n", sector, dirty);
				if (disk_write_sector(sector, (uint8_t *)sect) < 0)
				{
					fflush(stdout);
					perror("write_sector");
					return;
				}
				sector++;
				dirty = 0;
			}
			if (disk_read_sector(sector, (uint8_t *)sect) < 0)
			{
				fflush(stdout);
				perror("read_sector");
				return;
			}
			sector++;
		}
		
		if (!e3_block_is_in_block_group(sb, sect[pos].bg_block_bitmap, curbg))
		{
			printf("Block group %d block bitmap block (0x%08x) appears not to be in block group!  Resetting to default (0x%08x).\n",
				curbg, sect[pos].bg_block_bitmap, e3_block_group_expected_block_bitmap(sb, curbg));
			sect[pos].bg_block_bitmap = e3_block_group_expected_block_bitmap(sb, curbg);
			dirty++;
		}
		if (sect[pos].bg_block_bitmap != e3_block_group_expected_block_bitmap(sb, curbg))
			printf("Block group %d block bitmap block is 0x%08x, but expected %08x! Looks plausible otherwise, though; not fixing.\n", 
				curbg, sect[pos].bg_block_bitmap, e3_block_group_expected_block_bitmap(sb, curbg));
		
		if (!e3_block_is_in_block_group(sb, sect[pos].bg_inode_bitmap, curbg))
		{
			printf("Block group %d inode bitmap block (0x%08x) appears not to be in block group!  Resetting to default (0x%08x).\n",
				curbg, sect[pos].bg_inode_bitmap, e3_block_group_expected_inode_bitmap(sb, curbg));
			sect[pos].bg_inode_bitmap = e3_block_group_expected_inode_bitmap(sb, curbg);
			dirty++;
		}
		if (sect[pos].bg_inode_bitmap != e3_block_group_expected_inode_bitmap(sb, curbg))
			printf("Block group %d inode bitmap block is 0x%08x, but expected %08x! Looks plausible otherwise, though; not fixing.\n", 
				curbg, sect[pos].bg_inode_bitmap, e3_block_group_expected_inode_bitmap(sb, curbg));
		
		if (!e3_block_is_in_block_group(sb, sect[pos].bg_inode_table, curbg))
		{
			printf("Block group %d inode table start block (0x%08x) appears not to be in block group!  Resetting to default (0x%08x).\n",
				curbg, sect[pos].bg_inode_table, e3_block_group_expected_inode_table(sb, curbg));
			sect[pos].bg_inode_table = e3_block_group_expected_inode_table(sb, curbg);
			dirty++;
		}
		if (sect[pos].bg_inode_table != e3_block_group_expected_inode_table(sb, curbg))
			printf("Block group %d inode table start block is 0x%08x, but expected %08x! Looks plausible otherwise, though; not fixing.\n", 
				curbg, sect[pos].bg_inode_table, e3_block_group_expected_inode_table(sb, curbg));
	}
}
