// e3ls
// Utility to make sense out of really damaged ext2/ext3 filesystems.
//
// If you have to make an assumption, write it down. Better assumptions may
// lead to better grades.

#include <stdio.h>
#include <stdint.h>
#include <getopt.h>

#include "e3tools.h"
#include "diskio.h"
#include "inode.h"

static void _do_ls(e3tools_t *e3t, int ino, int rec, char *hdr)
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
				_do_ls(e3t, lld->inode, 1, rechdr);
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
		struct ext2_inode inode;
		
		ls_inode = strtoll(argv[arg], NULL, 0);
		
		inode_find(&e3t, ls_inode, &inode);
		_do_ls(&e3t, ls_inode, ls_recursive, "");
	}
	
	e3tools_close(&e3t);
	
	return 0;
}
