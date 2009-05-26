#ifndef _INODE_H
#define _INODE_H

extern void inode_table_show(struct ext2_super_block *sb, int bg);
void inode_table_check(struct ext2_super_block *sb, int bg);

#endif
