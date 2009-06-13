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
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

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
	/* XXX Leaks memory on failure. */
	sector_t sbsector = 2;
	char *diskdesc = strdup("recover");
	
	e3t->exceptions = NULL;
	e3t->cowfile = NULL;
	
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
			} else if (!strcmp((*argv)[arg], "--cowfile")) {
				_eat(arg, argc, argv);
				if (arg == *argc)
				{
					E3DEBUG(E3TOOLS_PFX "--cowfile requires a parameter!\n");
					return -1;
				}
				e3t->cowfile = strdup((*argv)[arg]);
				_eat(arg, argc, argv);
			} else if (!strcmp((*argv)[arg], "--disk")) {
				_eat(arg, argc, argv);
				if (arg == *argc)
				{
					E3DEBUG(E3TOOLS_PFX "--disk requires a parameter!\n");
					return -1;
				}
				free(diskdesc);
				diskdesc = strdup((*argv)[arg]);
				_eat(arg, argc, argv);
			} else
				arg++;
		}
	}
	
	if (disk_open(e3t, diskdesc) < 0)
	{
		E3DEBUG(E3TOOLS_PFX "Failed to open volume by name of \"%s\".\n", diskdesc);
		return -1;
	}
	
	if (e3t->cowfile)
		(void) diskcow_import(e3t, e3t->cowfile);	/* Failure is OK */
	
	E3DEBUG(E3TOOLS_PFX "Reading superblock from sector %lld.\n", (long long int)sbsector);
	if (disk_read_sector(e3t, sbsector, (uint8_t*)&e3t->sb) < 0)
	{
		perror("disk_read_sector(sbsector)");
		return -1;
	}
	
	free(diskdesc);
	
	return 0;
}

void e3tools_usage()
{
	printf("General e3tools options:\n");
	printf("--superblock <sector> chooses an alternative sector to read the filesystem's superblock from\n");
	printf("          -n <sector>\n");
	printf("--cowfile <file> gives a file to read in COW data from and save out COW data to\n");
	printf("--disk <mechanism> gives a mechanism by which to read a disk -- i.e., 'simple:recover' to read from a file called 'recover'.  This is the default.\n");
}

void e3tools_close(e3tools_t *e3t)
{
	disk_close(e3t);
	diskcow_export(e3t, e3t->cowfile);
	if (e3t->cowfile)
		free(e3t->cowfile);
}
