// e3view inode utilities
// Utility to make sense out of really damaged ext2/ext3 filesystems.
//
// If you have to make an assumption, write it down. Better assumptions may
// lead to better grades.

#include <stdio.h>
#include <linux/fs.h>
#include <linux/ext2_fs.h>

#include "diskio.h"
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
	
	printf("Inode table from block group %d\n", bg);
	printf("Starts at block %d, should contain %d inodes in %d blocks\n", curblock, inodes, blocks);
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
			int ino = i + b * inodes_per_block + bg * inodes + 1;
			
			printf("\tInode %d%s\n", ino,
				(ino == 1) ? " (badblocks)" :
				(ino == 2) ? " (root directory)" :
				(ino == 5) ? " (bootloader)" :
				(ino == 6) ? " (undelete)" :
				(ino == 7) ? " (extra group descriptors)" :
				(ino == 8) ? " (journal)" :
				"");
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
			printf("\t\tMode       : %04X (%s, %o)%s\n", inode->i_mode, ftype, inode->i_mode & 07777,
				(((inode->i_mode) & 0xF000 == 0) && inode->i_mode) ? " (looks bogus!)" : "");
			printf("\t\tLinks      : %d%s\n", inode->i_links_count,
				(inode->i_links_count > 1024) ? " (looks bogus!)" : "");
			printf("\t\tSize       : %d\n", inode->i_size);
			printf("\t\t# Blocks   : %d\n", inode->i_blocks);
			printf("\t\tBlock list : %d %d %d %d %d %d %d %d %d %d %d %d *%d **%d ***%d\n",
				inode->i_block[0], inode->i_block[1], inode->i_block[2], inode->i_block[3], 
				inode->i_block[4], inode->i_block[5], inode->i_block[6], inode->i_block[7], 
				inode->i_block[8], inode->i_block[9], inode->i_block[10], inode->i_block[11], 
				inode->i_block[12], inode->i_block[13], inode->i_block[14]);
			printf("\t\tFlags      : ");
			if (inode->i_flags & 0x00000001)
				printf("secrm ");
			if (inode->i_flags & 0x00000002)
				printf("unrm ");
			if (inode->i_flags & 0x00000004)
				printf("compr ");
			if (inode->i_flags & 0x00000008)
				printf("sync ");
			if (inode->i_flags & 0x00000010)
				printf("immutable ");
			if (inode->i_flags & 0x00000020)
				printf("append ");
			if (inode->i_flags & 0x00000040)
				printf("nodump ");
			if (inode->i_flags & 0x00000080)
				printf("noatime ");
			if (inode->i_flags & 0x00010000)
				printf("btreedir/hashdir ");
			if (inode->i_flags & 0x00020000)
				printf("afs ");
			if (inode->i_flags & 0x00040000)
				printf("journal ");
			if (inode->i_flags & 0xFFF80000)
				printf("type A? ");
			printf("\n");
		}
		curblock++;
	}
}

void inode_table_check(struct ext2_super_block *sb, int bg)
{
	int curblock = block_group_inode_table_block(sb, bg);
	int inodes = sb->s_inodes_per_group;
	int inodes_per_block = SB_BLOCK_SIZE(sb) / sb->s_inode_size;
	int blocks = inodes * sb->s_inode_size / SB_BLOCK_SIZE(sb);
	uint8_t *block = alloca(SB_BLOCK_SIZE(sb));
	int b;
	int ok = 0;
	int bogus = 0;
	
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

			if ((inode->i_links_count > 4096) || 
			    (((inode->i_mode) & 0xF000 == 0) && inode->i_mode))
				bogus++;
			else
				ok++;
		}
		curblock++;
	}
	printf("Inode table from block group %d: %d OK inodes, %d bogus inodes\n", bg, ok, bogus);
}
