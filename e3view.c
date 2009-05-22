// e3view
// Utility to make sense out of really damaged ext2/ext3 filesystems.
//
// If you have to make an assumption, write it down. Better assumptions may
// lead to better grades.
//
// Useful resources:
//  * http://www.nongnu.org/ext2-doc/ext2.html

#define VERSION "0.01"

#define _LARGEFILE64_SOURCE
#include <linux/fs.h>
#include <linux/ext2_fs.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdint.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

#include "diskio.h"
#include "e3bits.h"
#include "superblock.h"

void show_block_group_desc_table(struct ext2_super_block *sb)
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

void repair_block_group_desc_table(struct ext2_super_block *sb)
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



int main(int argc, char **argv)
{
	struct ext2_super_block sb;
	sector_t sbsector = 2;
	int opt;
	int print_sb = 0;
	int print_bgd = 0;
	int repair_bgd = 0;
	
	printf("e3view " VERSION "\n"
	       "This program does really dangerous things to really badly hosed drives.\n"
	       "Using it without care might make your drive into one of them. Create an\n"
	       "LVM snapshot before using this program.\n\n");
	
	while ((opt = getopt(argc, argv, "n:sdD")) != -1)
	{
		switch (opt)
		{
		case 'n':
			sbsector = strtoll(optarg, NULL, 0);
			break;
		case 's':
			print_sb = 1;
			break;
		case 'd':
			print_bgd = 1;
			break;
		case 'D':
			repair_bgd = 1;
			break;
		default:
			printf("Usage: %s [-n sector_of_alt_superblock] [-s] [-d] [-D]\n", argv[0]);
			printf("-n causes the superblock to be read from an alternate location\n");
			printf("-s enables printing of Superblock information\n");
			printf("-d enables printing of block group Descriptor information\n");
			printf("-D enables repair of block group Descriptor information\n");
			exit(1);
		}
	}
	
	printf("Reading superblock from sector %lld.\n", sbsector);
	if (disk_read_sector(sbsector, (uint8_t*)&sb) < 0)
	{
		perror("read_sector");
		return 1;
	}
	
	if (print_sb)
	{
		printf("Dumping superblock.\n");
		superblock_show(&sb);
		printf("\n");
	}
	
	if (print_bgd)
	{
		printf("Dumping block group descriptor table.\n");
		show_block_group_desc_table(&sb);
		printf("\n");
	}
	
	if (repair_bgd)
	{
		printf("Repairing block group descriptor table as needed.\n");
		repair_block_group_desc_table(&sb);
		printf("\n");
	}
	
	return 0;
}