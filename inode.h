#ifndef _INODE_H
#define _INODE_H

#include <stdint.h>

#include "blockgroup.h"

struct ifile;	// opaque; defined in inode.c

extern void inode_table_show(struct ext2_super_block *sb, int bg);
void inode_table_check(struct ext2_super_block *sb, int bg);
void inode_print(struct ext2_super_block *sb, struct ext2_inode *inode, int ino);
int inode_find(struct ext2_super_block *sb, int ino, struct ext2_inode *inode);

struct ifile *ifile_open(struct ext2_super_block *sb, int ino);
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
