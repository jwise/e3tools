// e3view inode utilities
// Utility to make sense out of really damaged ext2/ext3 filesystems.
//
// If you have to make an assumption, write it down. Better assumptions may
// lead to better grades.

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <alloca.h>
#include <linux/fs.h>
#include <linux/ext2_fs.h>

#include "e3tools.h"
#include "diskio.h"
#include "superblock.h"
#include "blockgroup.h"
#include "inode.h"

void inode_print(e3tools_t *e3t, struct ext2_inode *inode, int ino)
{
	char *ftype;
	
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
		((((inode->i_mode) & 0xF000) == 0) && inode->i_mode) ? " (looks bogus!)" : "");
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

int inode_find(e3tools_t *e3t, int ino, struct ext2_inode *inode)
{
	int inodes_per_block = SB_BLOCK_SIZE(&e3t->sb) / e3t->sb.s_inode_size;
	int bg = (ino - 1) / e3t->sb.s_inodes_per_group;
	int curblock = block_group_inode_table_block(e3t, bg) + ((ino - 1) % e3t->sb.s_inodes_per_group) / inodes_per_block;
	int offset = e3t->sb.s_inode_size * ((ino - 1) % inodes_per_block);
	uint8_t *block = alloca(SB_BLOCK_SIZE(&e3t->sb));
	
	if (disk_read_block(e3t, curblock, (uint8_t *)block) < 0)
	{
		perror("inode_find: disk_read_block");
		return -1;
	}
	
	memcpy((void*)inode, block + offset, e3t->sb.s_inode_size);
	
	return 0;
}

void inode_table_show(e3tools_t *e3t, int bg)
{
	int curblock = block_group_inode_table_block(e3t, bg);
	int inodes = e3t->sb.s_inodes_per_group;
	int inodes_per_block = SB_BLOCK_SIZE(&e3t->sb) / e3t->sb.s_inode_size;
	int blocks = inodes * e3t->sb.s_inode_size / SB_BLOCK_SIZE(&e3t->sb);
	uint8_t *block = alloca(SB_BLOCK_SIZE(&e3t->sb));
	int b;
	
	printf("Inode table from block group %d\n", bg);
	printf("Starts at block %d, should contain %d inodes in %d blocks\n", curblock, inodes, blocks);
	for (b = 0; b < blocks; b++)
	{
		int i;
		if (disk_read_block(e3t, curblock, (uint8_t *)block) < 0)
		{
			perror("inode_table_show: disk_read_block");
			return;
		}
		for (i = 0; i < inodes_per_block; i++)
		{
			struct ext2_inode *inode = (struct ext2_inode *)(block + e3t->sb.s_inode_size * i);
			int ino = i + b * inodes_per_block + bg * inodes + 1;
			
			inode_print(e3t, inode, ino);
		}
		curblock++;
	}
}

void inode_table_check(e3tools_t *e3t, int bg)
{
	int curblock = block_group_inode_table_block(e3t, bg);
	int inodes = e3t->sb.s_inodes_per_group;
	int inodes_per_block = SB_BLOCK_SIZE(&e3t->sb) / e3t->sb.s_inode_size;
	int blocks = inodes * e3t->sb.s_inode_size / SB_BLOCK_SIZE(&e3t->sb);
	uint8_t *block = alloca(SB_BLOCK_SIZE(&e3t->sb));
	int b;
	int ok = 0;
	int bogus = 0;
	
	for (b = 0; b < blocks; b++)
	{
		int i;
		if (disk_read_block(e3t, curblock, (uint8_t *)block) < 0)
		{
			perror("inode_table_check: disk_read_block");
			return;
		}
		for (i = 0; i < inodes_per_block; i++)
		{
			struct ext2_inode *inode = (struct ext2_inode *)(block + e3t->sb.s_inode_size * i);

			if ((inode->i_links_count > 4096) || 
			    ((((inode->i_mode) & 0xF000) == 0) && inode->i_mode))
				bogus++;
			else
				ok++;
		}
		curblock++;
	}
	printf("Inode table from block group %d: %d OK inodes, %d bogus inodes\n", bg, ok, bogus);
}

struct ifile {
	e3tools_t *e3t;
	struct ext2_inode inode;
	block_t curblock;
	int blockofs;
};

struct ifile *ifile_open(e3tools_t *e3t, int ino)
{
	struct ifile *ifp = malloc(sizeof(*ifp));
	
	if (!ifp)
		return NULL;
	
	if (inode_find(e3t, ino, &ifp->inode) < 0)
	{
		free(ifp);
		return NULL;
	}
	
	ifp->e3t = e3t;
	ifp->curblock = 0;
	ifp->blockofs = 0;
	
	return ifp;
}

static block_t _iblock_lookup(e3tools_t *e3t, struct ext2_inode *inode, int blockno)
{
	block_t *block = alloca(SB_BLOCK_SIZE(&e3t->sb));
	int perblk = SB_BLOCK_SIZE(&e3t->sb) / sizeof(block_t);
	int origblockno = blockno;
	
	/* Direct block? */
	if (blockno < INODE_INDIRECT1)
		return inode->i_block[blockno];
	
	/* Well, maybe in a first-level indirect block? */
	blockno -= INODE_INDIRECT1;
	if (blockno < perblk)
	{
		if (inode->i_block[INODE_INDIRECT1] == 0)	/* Ha! Gotcha! */
			return 0;
		if (disk_read_block(e3t, inode->i_block[INODE_INDIRECT1], (uint8_t *)block) < 0)
		{
			perror("_iblock_lookup: disk_read_block(INDIRECT1)");
			return;
		}
		return block[blockno];
	}
	
	/* How about in a second-level indirect block? */
	blockno -= perblk;
	if (blockno < (perblk * perblk))
	{
		if (inode->i_block[INODE_INDIRECT2] == 0)	/* Ha! Gotcha! */
			return 0;
		if (disk_read_block(e3t, inode->i_block[INODE_INDIRECT2], (uint8_t *)block) < 0)
		{
			perror("_iblock_lookup: disk_read_block(INDIRECT2)");
			return;
		}
		
		if (block[blockno / perblk] == 0)		/* Ha! Gotcha! */
			return 0;
		if (disk_read_block(e3t, block[blockno / perblk], (uint8_t *)block) < 0)
		{
			perror("_iblock_lookup: disk_read_block(*INDIRECT2)");
			return;
		}
		return block[blockno % perblk];
	}
	
	/* Maybe a third level indirect block? */
	blockno -= perblk * perblk;
	if (blockno < (perblk * perblk * perblk))
	{
		if (inode->i_block[INODE_INDIRECT3] == 0)	/* Ha! Gotcha! */
			return 0;
		if (disk_read_block(e3t, inode->i_block[INODE_INDIRECT3], (uint8_t *)block) < 0)
		{
			perror("_iblock_lookup: disk_read_block(INDIRECT3)");
			return;
		}
		
		if (block[blockno / (perblk * perblk)] == 0)	/* Ha! Gotcha !*/
			return 0;
		if (disk_read_block(e3t, block[blockno / (perblk * perblk)], (uint8_t *)block) < 0)
		{
			perror("_iblock_lookup: disk_read_block(*INDIRECT3)");
			return;
		}
		
		if (block[blockno % (perblk * perblk)] == 0)	/* Ha! Gotcha !*/
			return 0;
		if (disk_read_block(e3t, block[blockno % (perblk * perblk)], (uint8_t *)block) < 0)
		{
			perror("_iblock_lookup: disk_read_block(**INDIRECT3)");
			return;
		}
		return block[blockno % perblk];
	}
	
	printf("_iblock_lookup: WTF? requested blockno %d is not in either the inode, the first level indirect, the second level indirect, *or* the third level indirect...\n", origblockno);
	return 0;
}

int ifile_read(struct ifile *ifp, char *buf, int len)
{
	int rlen = 0;
	int blocksz = SB_BLOCK_SIZE(&ifp->e3t->sb);
	uint64_t curpos = U64(ifp->curblock) * U64(blocksz) + U64(ifp->blockofs);
	uint64_t flen = INODE_FILE_SIZE(&ifp->inode);
	uint8_t *block = alloca(SB_BLOCK_SIZE(&ifp->e3t->sb));
	
	while (len)
	{
		block_t diskblock;
		int nbytes = blocksz - ifp->blockofs;	/* We can transfer a maximum of one block each go. */
		
		if ((curpos + U64(nbytes)) > flen)
			nbytes = flen - curpos;
		if (nbytes > len)
			nbytes = len;
		if (nbytes == 0)
			break;
		
		/* Next up, see if we can find the block in the inode's table. */
		diskblock = _iblock_lookup(ifp->e3t, &ifp->inode, ifp->curblock);
		if (diskblock == 0)	/* Sparse block -- fill in the blanks */
		{
			memset(buf, 0, nbytes);
			buf += nbytes;
		} else {
			if (disk_read_block(ifp->e3t, diskblock, block) < 0)
			{
				perror("_ifile_read: disk_read_block");
				return -1;
			}
			memcpy(buf, block + ifp->blockofs, nbytes);
			buf += nbytes;
		}
		
		ifp->blockofs += nbytes;
		if (ifp->blockofs == blocksz)
		{
			ifp->blockofs = 0;
			ifp->curblock++;
		}
		curpos += nbytes;
		rlen += nbytes;
		len -= nbytes;
	}
	return rlen;
}

void ifile_close(struct ifile *ifp)
{
	free(ifp);
}
