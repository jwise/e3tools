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

#include "e3tools.h"
#include "diskio.h"
#include "e3bits.h"
#include "superblock.h"
#include "blockgroup.h"
#include "inode.h"

void ls(e3tools_t *e3t, int ino, int rec, char *hdr)
{
	struct ifile *ifp;
	char lol[32768];	/* XXX */
	struct lld {		/* XXX */
		uint32_t inode;
		uint16_t rec_len;
		unsigned char name_len;
		char file_type;
		char name[256];
	};
	struct lld *lld;
	int totpos = 0;
	int totlen;
	char *rechdr = malloc(strlen(hdr) + 3);
	strcpy(rechdr, hdr);
	strcat(rechdr, "  ");
	
	ifp = ifile_open(e3t, ino);
	if (!ifp)
	{
		printf("open failure!\n");
		return;
	}
	totlen = ifile_read(ifp, lol, 32768);
	
	while (totpos < totlen)
	{
		lld = (struct lld *)(lol + totpos);
		totpos += lld->rec_len;
		char fname[256];
		
		if (lld->rec_len == 0)
		{
			printf("%sYIKES that looks bad! rec_len = 0?\n", hdr);
			break;
		}
		
		strncpy(fname, lld->name, lld->name_len);
		fname[lld->name_len] = 0;
		
		switch (lld->file_type)
		{
		case 0:
			printf("%s%d bytes padding\n", hdr, lld->rec_len);
			break;
		case 1:
			printf("%s[FIL@%d] %s\n", hdr, lld->inode, fname);
			break;
		case 2:
			printf("%s[DIR@%d] %s\n", hdr, lld->inode, fname);
			if (rec && strcmp(fname, ".") && strcmp(fname, ".."))
				ls(e3t, lld->inode, 1, rechdr);
			break;
		default:
			printf("%s[???@%d] %s\n", hdr, lld->inode, fname);
			break;
		}
	}
	
	ifile_close(ifp);
	free(rechdr);
}

int main(int argc, char **argv)
{
	e3tools_t e3t;
	int opt;
	int print_sb = 0;
	int print_bgd = 0;
	int repair_bgd = 0;
	int check_itable = 0;
	int show_itable = -1;
	int show_inode = -1;
	int ls_inode = -1;
	int ls_recursive = 0;
	int i;
	
	printf("e3view " VERSION "\n"
	       "This program does really dangerous things to really badly hosed drives.\n"
	       "Using it without care might make your drive into one of them. Create an\n"
	       "LVM snapshot before using this program.\n\n");
	      
	if (e3tools_init(&e3t, &argc, &argv) < 0)
	{
		printf("e3tools initialization failed -- bailing out\n");
		return 1;
	}
	
	while ((opt = getopt(argc, argv, "sdDTt:i:l:R")) != -1)
	{
		switch (opt)
		{
		case 's':
			print_sb = 1;
			break;
		case 'd':
			print_bgd = 1;
			break;
		case 'D':
			repair_bgd = 1;
			break;
		case 'T':
			check_itable = 1;
			break;
		case 't':
			show_itable = strtoll(optarg, NULL, 0);
			break;
		case 'i':
			show_inode = strtoll(optarg, NULL, 0);
			break;
		case 'l':
			ls_inode = strtoll(optarg, NULL, 0);
			break;
		case 'R':
			ls_recursive = 1;
			break;
		default:
			printf("Usage: %s [-s] [-d] [-D] [-t block_group] [-T] [-i inode] [-l inode] [-R]\n", argv[0]);
			printf("-s enables printing of Superblock information\n");
			printf("-d enables printing of block group Descriptor information\n");
			printf("-D enables repair of block group Descriptor information\n");
			printf("-T enables check/salvage of inode table information\n");
			printf("-t displays the contents of a block group's inode table\n");
			printf("-i displays the contents of an inode\n");
			printf("-l gives a 'ls' of a directory inode\n");
			printf("-R makes 'ls' recursive\n");
			e3tools_usage();
			exit(1);
		}
	}
	
	if (print_sb)
	{
		printf("Dumping superblock.\n");
		superblock_show(&e3t);
		printf("\n");
	}
	
	if (print_bgd)
	{
		printf("Dumping block group descriptor table.\n");
		block_group_desc_table_show(&e3t);
		printf("\n");
	}
	
	if (repair_bgd)
	{
		printf("Repairing block group descriptor table as needed.\n");
		block_group_desc_table_repair(&e3t);
		printf("\n");
	}
	
	if (show_itable >= 0)
		inode_table_show(&e3t, show_itable);
	
	if (check_itable)
		for (i = 0; i < SB_GROUPS(&e3t.sb); i++)
			inode_table_check(&e3t, i);
	
	if (show_inode >= 0)
	{
		struct ext2_inode inode;
		inode_find(&e3t, show_inode, &inode);
		inode_print(&e3t, &inode, show_inode);
	}
	
	if (ls_inode >= 0)
	{
		struct ext2_inode inode;
		inode_find(&e3t, ls_inode, &inode);
		ls(&e3t, ls_inode, ls_recursive, "");
	}
	
	return 0;
}
