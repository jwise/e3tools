// e3ls
// Utility to make sense out of really damaged ext2/ext3 filesystems.
//
// If you have to make an assumption, write it down. Better assumptions may
// lead to better grades.

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <getopt.h>
#include <alloca.h>

#include "e3tools.h"
#include "diskio.h"
#include "inode.h"

#define EXT3_LL_MAX_NAME 256
typedef struct ext3_lldir {		/* XXX */
	uint32_t inode;
	uint16_t rec_len;
	unsigned char name_len;
	char file_type;
	char name[EXT3_LL_MAX_NAME];
} ext3_lldir_t;

static void _print_entry(e3tools_t *e3t, ext3_lldir_t *lld)
{
	char fname[EXT3_LL_MAX_NAME];
	char *type;
	
	strncpy(fname, lld->name, lld->name_len);
	fname[lld->name_len] = 0;
	
	if (lld->file_type == 0)
	{
		printf("%d bytes padding\n", lld->rec_len);
		return;
	}
	
	if (lld->inode == 0)
		printf("DELETED ");
	
	switch (lld->file_type)
	{
	case 1: type = "FIL"; break;
	case 2: type = "DIR"; break;
	case 3: type = "CHR"; break;
	case 4: type = "BLK"; break;
	case 5: type = "FIF"; break;
	case 6: type = "SCK"; break;
	case 7: type = "SYM"; break;
	default: type = "???"; break;
	}
	
	printf("[%s@%d, %d] %s\n", type, lld->inode, lld->rec_len, fname);
}

static void _do_ls(e3tools_t *e3t, int ino, int recdepth)
{
	struct ifile *ifp;
	ext3_lldir_t *lld;
	int blkpos = 0;
	int blklen;
	void *block = alloca(SB_BLOCK_SIZE(&e3t->sb));
	
	ifp = ifile_open(e3t, ino);
	if (!ifp)
	{
		printf("Directory inode %d open failure!\n", ino);
		return;
	}
	
	while ((blklen = ifile_read(ifp, block, SB_BLOCK_SIZE(&e3t->sb))) > 0)
	{
		blkpos = 0;
		if (blklen != SB_BLOCK_SIZE(&e3t->sb))
			printf("WARNING: directory inode %d short read (%d bytes) -- inode on fire?\n", ino, blklen);

		while (blkpos < blklen)
		{
			int i;
			
			for (i = 0; i < recdepth; i++)	/* Disambiguate directory levels. */
				printf("  ");
			
			lld = (ext3_lldir_t *)(block + blkpos);
			blkpos += lld->rec_len;
			
			if (lld->rec_len == 0)
			{
				printf("WARNING: directory inode %d has a record where rec_len = 0 -- inode on fire?\n", ino);
				goto bailout;
			}
			
			_print_entry(e3t, lld);
			if ((lld->file_type == 2 /* directory */) && (recdepth >= 0) && strncmp(lld->name, ".", 2) && strncmp(lld->name, "..", 2) && lld->inode)
				_do_ls(e3t, lld->inode, recdepth + 1);
		}
		
		if (blkpos != blklen)
			printf("WARNING: directory inode %d padding overran a single block -- inode on fire?\n", ino);
	}
	if (blklen < 0)
		printf("WARNING: directory inode %d read failure -- inode on fire?\n", ino);

bailout:
	ifile_close(ifp);
}

int main(int argc, char **argv)
{
	e3tools_t e3t;
	int opt;
	int arg;
	int ls_inode;
	int ls_recursive = 0;
	
	if (e3tools_init(&e3t, &argc, &argv) < 0)
	{
		printf("e3tools initialization failed -- bailing out\n");
		return 1;
	}
	
	while ((opt = getopt(argc, argv, "R")) != -1)
	{
		switch (opt)
		{
		case 'R':
			ls_recursive = 1;
			break;
		default:
			printf("Usage: %s [-R] inodes...\n", argv[0]);
			printf("-R enables recursive behavior\n");
			e3tools_usage();
			exit(1);
		}
	}
	
	for (arg = optind; arg < argc; arg++)
	{
		ls_inode = strtoll(argv[arg], NULL, 0);
		
		printf("Directory listing for inode %d:\n", ls_inode);
		_do_ls(&e3t, ls_inode, ls_recursive ? 0 : -1);
	}
	
	e3tools_close(&e3t);
	
	return 0;
}
