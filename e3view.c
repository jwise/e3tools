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
#include <assert.h>

#include "diskio.h"
#include "e3bits.h"
#include "superblock.h"
#include "blockgroup.h"

void inode_table_show(struct ext2_super_block *sb, int bg)
{
	int curblock = block_group_inode_table_block(sb, bg);
	int inodes = sb->s_inodes_per_group;
	int inodes_per_block = SB_BLOCK_SIZE(sb) / sb->s_inode_size;
	int blocks = inodes * sb->s_inode_size / SB_BLOCK_SIZE(sb);
	uint8_t *block = alloca(SB_BLOCK_SIZE(sb));
	int b;
	int ok = 0;
	int bogus = 0;
	
	printf("Inode table from block group %d\n", bg);
	//printf("Starts at block %d, should contain %d inodes in %d blocks\n", curblock, inodes, blocks);
	for (b = 0; b < blocks; b++)
	{
		int i;
		if (disk_read_block(sb, curblock, (uint8_t *)block) < 0)
		{
			perror("disk_read_block");
			return;
		}
		for (i = 0; i < inodes_per_block; i++)
		{
			struct ext2_inode *inode = (struct ext2_inode *)(block + sb->s_inode_size * i);
			char *ftype;
			
		//	printf("\tInode %d\n", i + b * inodes_per_block + bg * inodes + 1);
			switch (inode->i_mode & 0xF000)
			{
			case 0x0000: ftype = "empty"; break;
			case 0x1000: ftype = "FIFO"; break;
			case 0x2000: ftype = "character device"; break;
			case 0x4000: ftype = "directory"; break;
			case 0x6000: ftype = "block device"; break;
			case 0x8000: ftype = "regular file"; break;
			case 0xA000: ftype = "symbolic link"; break;
			case 0xC000: ftype = "socket"; break;
			default: ftype = "bogus file type!"; break;
			}
		//	printf("\t\tMode   : %04X (%s, %o)\n", inode->i_mode, ftype, inode->i_mode & 07777);
		//	printf("\t\tLinks  : %d%s\n", inode->i_links_count,
		//		(inode->i_links_count > 1024) ? " (looks bogus!)" : " (looks OK)");
		//	printf("\t\tBlocks : %d\n", inode->i_blocks);
			if (inode->i_links_count > 4096)
				bogus++;
			else
				ok++;
		}
		curblock++;
	}
	printf("%d OK inodes, %d bogus inodes\n", ok, bogus);
}

int main(int argc, char **argv)
{
	struct ext2_super_block sb;
	sector_t sbsector = 2;
	int opt;
	int print_sb = 0;
	int print_bgd = 0;
	int repair_bgd = 0;
	int i;
	
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
		block_group_desc_table_show(&sb);
		printf("\n");
	}
	
	if (repair_bgd)
	{
		printf("Repairing block group descriptor table as needed.\n");
		block_group_desc_table_repair(&sb);
		printf("\n");
	}
	
	for (i = 0; i < SB_GROUPS(&sb); i++)
		inode_table_show(&sb, i);
	
	return 0;
}