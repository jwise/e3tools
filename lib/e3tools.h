#ifndef _E3TOOLS_H
#define _E3TOOLS_H

#include <linux/fs.h>
#include <linux/ext2_fs.h>

#define E3DEBUG(...) fprintf(stderr, __VA_ARGS__)

#define E3TOOLS_VERSION "0.01"
#define E3TOOLS_NAME "e3tools"
#define E3TOOLS_PFX E3TOOLS_NAME ": "
#define E3TOOLS_DBG_DISKIO 0x00000001

typedef struct e3tools e3tools_t;

#include "diskio.h"

struct e3tools {
	struct ext2_super_block sb;
	struct exception *exceptions;
	char *cowfile;
	diskio_t *disk;
	unsigned long debug;
};

extern int e3tools_init(e3tools_t *e3t, int *argc, char ***argv);
extern void e3tools_usage();
extern void e3tools_close(e3tools_t *e3t);

#endif
