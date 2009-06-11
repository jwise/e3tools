// e3dumpblock
// Utilities to make sense out of really damaged ext2/ext3 filesystems.
//
// If you have to make an assumption, write it down. Better assumptions may
// lead to better grades.

#include <stdlib.h>
#include <stdio.h>

#include "e3tools.h"
#include "superblock.h"
#include "diskio.h"

int main(int argc, char **argv)
{
	e3tools_t e3t;
	block_t b;
	void *block;
	
	if (e3tools_init(&e3t, &argc, &argv) < 0)
	{
		printf("e3tools initialization failed -- bailing out\n");
		return 1;
	}
	
	if (argc != 2)
	{
		printf("Usage: %s e3tools_options\n", argv[0]);
		e3tools_usage();
		exit(1);
	}
	
	block = alloca(SB_BLOCK_SIZE(&e3t.sb));
	b = strtoll(argv[1], NULL, 0);
	
	if (disk_read_block(&e3t, b, (uint8_t *)block) < 0)
	{
		perror("disk_read_block");
		return 1;
	}
	
	write(1, block, SB_BLOCK_SIZE(&e3t.sb));
	
	e3tools_close(&e3t);
	
	return 0;
}
