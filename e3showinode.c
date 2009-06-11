// e3showinode
// Utilities to make sense out of really damaged ext2/ext3 filesystems.
//
// If you have to make an assumption, write it down. Better assumptions may
// lead to better grades.

#include <stdlib.h>
#include <stdio.h>

#include "e3tools.h"
#include "superblock.h"

int main(int argc, char **argv)
{
	e3tools_t e3t;
	struct ext2_inode inode;
	int inum;
	int arg;
	
	if (e3tools_init(&e3t, &argc, &argv) < 0)
	{
		printf("e3tools initialization failed -- bailing out\n");
		return 1;
	}
	
	if (argc < 2)
	{
		printf("Usage: %s e3tools_options inode_number\n", argv[0]);
		e3tools_usage();
		exit(1);
	}
	
	for (arg = 1; arg < argc; arg++)
	{
		inum = strtoll(argv[arg], NULL, 0);
		
		if (inode_find(&e3t, inum, &inode) < 0)
		{
			printf("Error reading inode %d\n", inum);
			exit(1);
		}
		inode_print(&e3t, &inode, inum);
	}
	
	e3tools_close(&e3t);
	
	return 0;
}
