#ifndef _INODE_H
#define _INODE_H

#include <stdint.h>

#include "e3tools.h"
#include "blockgroup.h"

struct ifile;	// opaque; defined in inode.c

extern void inode_table_show(e3tools_t *e3t, int bg);
void inode_table_check(e3tools_t *e3t, int bg);
void inode_print(e3tools_t *e3t, struct ext2_inode *inode, int ino);
int inode_find(e3tools_t *e3t, int ino, struct ext2_inode *inode);

struct ifile *ifile_open(e3tools_t *e3t, int ino);
int ifile_read(struct ifile *ifp, char *buf, int len);
void ifile_close(struct ifile *ifp);

#ifndef U64
#  define U64(x) ((uint64_t)(x))
#endif
#define INODE_FILE_SIZE(inode) ((((inode)->i_mode & 0xF000) == 0x8000) ? \
                                 (U64((inode)->i_size) | (U64((inode)->i_dir_acl) << 32)) : \
                                 (U64((inode)->i_size)))

#define INODE_INDIRECT1 12
#define INODE_INDIRECT2 13
#define INODE_INDIRECT3 14

#endif
