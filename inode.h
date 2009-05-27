#ifndef _INODE_H
#define _INODE_H

extern void inode_table_show(struct ext2_super_block *sb, int bg);
void inode_table_check(struct ext2_super_block *sb, int bg);
void inode_print(struct ext2_super_block *sb, struct ext2_inode *inode, int ino);
int inode_find(struct ext2_super_block *sb, int ino, struct ext2_inode *inode);

#endif
