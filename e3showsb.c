// e3showsb
// Utilities to make sense out of really damaged ext2/ext3 filesystems.
//
// If you have to make an assumption, write it down. Better assumptions may
// lead to better grades.

#include "e3tools.h"
#include "superblock.h"

int main(int argc, char **argv)
{
	e3tools_t e3t;
	
	if (e3tools_init(&e3t, &argc, &argv) < 0)
	{
		printf("e3tools initialization failed -- bailing out\n");
		return 1;
	}
	
	if (argc > 1)
	{
		printf("Usage: %s e3tools_options\n", argv[0]);
		e3tools_usage();
		exit(1);
	}
	
	superblock_show(&e3t);
	
	disk_close(&e3t);
	
	return 0;
}
