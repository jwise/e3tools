// e3tools miscellaneous library routines
// Utility to make sense out of really damaged ext2/ext3 filesystems.
//
// If you have to make an assumption, write it down. Better assumptions may
// lead to better grades.
//
// Useful resources:
//  * http://www.nongnu.org/ext2-doc/ext2.html

#include <linux/fs.h>
#include <linux/ext2_fs.h>
#include <stdio.h>

#include "e3tools.h"
#include "diskio.h"

static void _eat(int arg, int *argc, char ***argv)
{
	(*argc)--;
	for (; arg < *argc; arg++)
		(*argv)[arg] = (*argv)[arg+1];
}

int e3tools_init(e3tools_t *e3t, int *argc, char ***argv)
{
	sector_t sbsector = 2;
	
	e3t->exceptions = NULL;
	e3t->diskfd = -1;
	
	/* I do not like this 'nomming options' thing, since it means I have
	 * to essentially reinvent getopt.  It appears to be what gtk does
	 * though, with gtk_parse_args.  Megaloss. */
	if (argc && argv)
	{
		int arg;
		for (arg = 1; arg < *argc; /* arg incremented by eater */)
		{
			if (!strcmp((*argv)[arg], "--superblock") ||
			    !strcmp((*argv)[arg], "-n"))
			{
				_eat(arg, argc, argv);
				if (arg == *argc)
				{
					E3DEBUG(E3TOOLS_PFX "--superblock requires a parameter!\n");
					return -1;
				}
				sbsector = strtoll((*argv)[arg], NULL, 0);
				_eat(arg, argc, argv);
			} else
				arg++;
		}
	}
	
	E3DEBUG(E3TOOLS_PFX "Reading superblock from sector %lld.\n", sbsector);
	if (disk_read_sector(e3t, sbsector, (uint8_t*)&e3t->sb) < 0)
	{
		perror("disk_read_sector(sbsector)");
		return -1;
	}
	
	return 0;
}

void e3tools_usage()
{
	printf("General e3tools options:\n");
	printf("--superblock <sector> chooses an alternative sector to read the filesystem's superblock from\n");
	printf("          -n <sector>\n");
}
