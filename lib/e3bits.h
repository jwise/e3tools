#ifndef _E3BITS_H
#define _E3BITS_H

static inline int __ispow(int n, int p)
{
	if (n == 1)
		return 1;
	if (n % p)
		return 0;
	return __ispow(n / p, p);
}

static inline int e3_sb_blocks(struct ext2_super_block *sb)
{
	int bgs = sb->s_blocks_count / sb->s_blocks_per_group;
	if (sb->s_blocks_count % sb->s_blocks_per_group)
		bgs++;
	int bytes_per_block = 1024 << sb->s_log_block_size;
	int bgblocks = sizeof(struct ext2_group_desc) * (bgs) / bytes_per_block;
	if (sizeof(struct ext2_group_desc) * (bgs) % bytes_per_block)
		bgblocks++;
	return 1 /* Superblock */ + /*bgblocks*/ 0x400 /* wtf? */;
}

static inline int e3_block_group_has_sb(struct ext2_super_block *sb, int bg)
{
	if (!(sb->s_feature_ro_compat & EXT2_FEATURE_RO_COMPAT_SPARSE_SUPER))
		return 1;
	return (bg == 0) || (bg == 1) || __ispow(bg, 3) || __ispow(bg, 5) || __ispow(bg, 7);
}

static inline int e3_block_is_in_block_group(struct ext2_super_block *sb, int block, int bg)
{
	return (block / sb->s_blocks_per_group) == bg;
}

static inline int e3_block_group_expected_block_bitmap(struct ext2_super_block *sb, int bg)
{
	return bg * sb->s_blocks_per_group + (e3_block_group_has_sb(sb, bg) ? e3_sb_blocks(sb) : 0);
}

static inline int e3_block_group_expected_inode_bitmap(struct ext2_super_block *sb, int bg)
{
	return e3_block_group_expected_block_bitmap(sb, bg) + 1;
}

static inline int e3_block_group_expected_inode_table(struct ext2_super_block *sb, int bg)
{
	return e3_block_group_expected_block_bitmap(sb, bg) + 2;
}

#endif
