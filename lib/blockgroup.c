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

block_t block_group_inode_table_block(e3tools_t *e3t, int bg)
{
	int sectors_per_block = (1024 / BYTES_PER_SECTOR) << e3t->sb.s_log_block_size;
	sector_t sector = (e3t->sb.s_block_group_nr * e3t->sb.s_blocks_per_group + 1LL) * (sector_t)sectors_per_block;
	struct ext2_group_desc sect[BYTES_PER_SECTOR / sizeof(struct ext2_group_desc)];
	
	sector += (bg * sizeof(struct ext2_group_desc)) / BYTES_PER_SECTOR;
	
	if (disk_read_sector(e3t, sector, (uint8_t *)sect) < 0)
	{
		fflush(stdout);
		perror("read_sector");
		return -1;
	}
	
	return sect[bg % (BYTES_PER_SECTOR / sizeof(struct ext2_group_desc))].bg_inode_table;
}

void block_group_desc_table_show(e3tools_t *e3t)
{
	int bgs = e3t->sb.s_blocks_count / e3t->sb.s_blocks_per_group;
	int bytes_per_block = 1024 << e3t->sb.s_log_block_size;
	int sectors_per_block = (1024 / BYTES_PER_SECTOR) << e3t->sb.s_log_block_size;
	int curbg;
	sector_t sector;
	struct ext2_group_desc sect[BYTES_PER_SECTOR / sizeof(struct ext2_group_desc)];
	
	printf("Block descriptor table from block group %d\n", e3t->sb.s_block_group_nr);
	printf("Starts on block %d\n", e3t->sb.s_block_group_nr * e3t->sb.s_blocks_per_group + 1);
	sector = (e3t->sb.s_block_group_nr * e3t->sb.s_blocks_per_group + 1LL) * (sector_t)sectors_per_block;
	printf("  ... or sector %lld\n", (long long int)sector);
	printf("Expecting %d block groups\n", bgs);
	printf("Expecting %lu bytes of block group\n", sizeof(struct ext2_group_desc) * (bgs));
	printf("Expecting %lld sectors of block group\n", sizeof(struct ext2_group_desc) * (bgs) / BYTES_PER_SECTOR);
	printf("Expecting %lu blocks of block group\n", sizeof(struct ext2_group_desc) * (bgs) / bytes_per_block);
	printf("OK, let's do this!\n");
	printf("\n");
	
	for (curbg = 0; curbg < bgs; curbg++)
	{
		int pos = curbg % (BYTES_PER_SECTOR / sizeof(struct ext2_group_desc));
		if (pos == 0)
		{
			printf("Reading from new sector: %lld\n", (long long int)sector);
			if (disk_read_sector(e3t, sector, (uint8_t *)sect) < 0)
			{
				fflush(stdout);
				perror("read_sector");
				return;
			}
			sector++;
		}
		printf("\tBlock group %d\n", curbg);
		if (e3_block_group_has_sb(&e3t->sb, curbg))
			printf("\t\tHas superblock\n");

		printf("\t\tBitmap block : %12d (0x%08x)\n", sect[pos].bg_block_bitmap, sect[pos].bg_block_bitmap);
		if (!e3_block_is_in_block_group(&e3t->sb, sect[pos].bg_block_bitmap, curbg))
			printf("\t\t               ...looks bad!\n");
		if (sect[pos].bg_block_bitmap != e3_block_group_expected_block_bitmap(&e3t->sb, curbg))
			printf("\t\t               ...but expected %08x!\n", e3_block_group_expected_block_bitmap(&e3t->sb, curbg));

		printf("\t\tInode block  : %12d (0x%08x)\n", sect[pos].bg_inode_bitmap, sect[pos].bg_inode_bitmap);
		if (!e3_block_is_in_block_group(&e3t->sb, sect[pos].bg_inode_bitmap, curbg))
			printf("\t\t               ...looks bad!\n");
		if (sect[pos].bg_inode_bitmap != (e3_block_group_expected_block_bitmap(&e3t->sb, curbg) + 1))
			printf("\t\t               ...but expected %08x!\n", e3_block_group_expected_block_bitmap(&e3t->sb, curbg) + 1);

		printf("\t\tInode table  : %12d (0x%08x)\n", sect[pos].bg_inode_table, sect[pos].bg_inode_table);
		if (!e3_block_is_in_block_group(&e3t->sb, sect[pos].bg_inode_table, curbg))
			printf("\t\t               ...looks bad!\n");
		if (sect[pos].bg_inode_table != (e3_block_group_expected_block_bitmap(&e3t->sb, curbg) + 2))
			printf("\t\t               ...but expected %08x!\n", e3_block_group_expected_block_bitmap(&e3t->sb, curbg) + 2);
	}
}

void block_group_desc_table_repair(e3tools_t *e3t)
{
	int bgs = e3t->sb.s_blocks_count / e3t->sb.s_blocks_per_group;
	int sectors_per_block = (1024 / BYTES_PER_SECTOR) << e3t->sb.s_log_block_size;
	int curbg;
	sector_t sector;
	struct ext2_group_desc sect[BYTES_PER_SECTOR / sizeof(struct ext2_group_desc)];
	int dirty = 0;
	
	sector = (e3t->sb.s_block_group_nr * e3t->sb.s_blocks_per_group + 1LL) * (sector_t)sectors_per_block;
	for (curbg = 0; curbg < bgs; curbg++)
	{
		int pos = curbg % (BYTES_PER_SECTOR / sizeof(struct ext2_group_desc));
		if (pos == 0)
		{
			if (dirty)
			{
				sector--;
				printf("Writing back to repaired sector: %lld (%d changes)\n", (long long int)sector, dirty);
				if (disk_write_sector(e3t, sector, (uint8_t *)sect) < 0)
				{
					fflush(stdout);
					perror("write_sector");
					return;
				}
				sector++;
				dirty = 0;
			}
			if (disk_read_sector(e3t, sector, (uint8_t *)sect) < 0)
			{
				fflush(stdout);
				perror("read_sector");
				return;
			}
			sector++;
		}
		
		if (!e3_block_is_in_block_group(&e3t->sb, sect[pos].bg_block_bitmap, curbg))
		{
			printf("Block group %d block bitmap block (0x%08x) appears not to be in block group!  Resetting to default (0x%08x).\n",
				curbg, sect[pos].bg_block_bitmap, e3_block_group_expected_block_bitmap(&e3t->sb, curbg));
			sect[pos].bg_block_bitmap = e3_block_group_expected_block_bitmap(&e3t->sb, curbg);
			dirty++;
		}
		if (sect[pos].bg_block_bitmap != e3_block_group_expected_block_bitmap(&e3t->sb, curbg))
			printf("Block group %d block bitmap block is 0x%08x, but expected %08x! Looks plausible otherwise, though; not fixing.\n", 
				curbg, sect[pos].bg_block_bitmap, e3_block_group_expected_block_bitmap(&e3t->sb, curbg));
		
		if (!e3_block_is_in_block_group(&e3t->sb, sect[pos].bg_inode_bitmap, curbg))
		{
			printf("Block group %d inode bitmap block (0x%08x) appears not to be in block group!  Resetting to default (0x%08x).\n",
				curbg, sect[pos].bg_inode_bitmap, e3_block_group_expected_inode_bitmap(&e3t->sb, curbg));
			sect[pos].bg_inode_bitmap = e3_block_group_expected_inode_bitmap(&e3t->sb, curbg);
			dirty++;
		}
		if (sect[pos].bg_inode_bitmap != e3_block_group_expected_inode_bitmap(&e3t->sb, curbg))
			printf("Block group %d inode bitmap block is 0x%08x, but expected %08x! Looks plausible otherwise, though; not fixing.\n", 
				curbg, sect[pos].bg_inode_bitmap, e3_block_group_expected_inode_bitmap(&e3t->sb, curbg));
		
		if (!e3_block_is_in_block_group(&e3t->sb, sect[pos].bg_inode_table, curbg))
		{
			printf("Block group %d inode table start block (0x%08x) appears not to be in block group!  Resetting to default (0x%08x).\n",
				curbg, sect[pos].bg_inode_table, e3_block_group_expected_inode_table(&e3t->sb, curbg));
			sect[pos].bg_inode_table = e3_block_group_expected_inode_table(&e3t->sb, curbg);
			dirty++;
		}
		if (sect[pos].bg_inode_table != e3_block_group_expected_inode_table(&e3t->sb, curbg))
			printf("Block group %d inode table start block is 0x%08x, but expected %08x! Looks plausible otherwise, though; not fixing.\n", 
				curbg, sect[pos].bg_inode_table, e3_block_group_expected_inode_table(&e3t->sb, curbg));
	}
}
