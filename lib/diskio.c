// e3tools disk I/O layer
// Utility to make sense out of really damaged ext2/ext3 filesystems.
//
// If you have to make an assumption, write it down. Better assumptions may
// lead to better grades.

/* The general idea behind this file is that disk_read and disk_write
 * abstract away the bits that go read from the RAID partition or from the
 * LVM inside the RAID.  Later, it will be possible to specify a 'chain' of
 * procedures to call (i.e.,
 * "raid:storagefixa:storagefixb:storagefixc,lvm:storage/storage0"), but for
 * now the access methods will be hardcoded.
 *
 * Eventually, there will be a mechanism by which to mark sectors as
 * "insane" (disk_insane()?), which will by some indeterminate mechanism
 * cause the caller to retry the read with a different path to disk (or
 * cause the caller to just report the sector in question as inaccessible).
 */

#define _LARGEFILE64_SOURCE
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdint.h>
#include <unistd.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "e3tools.h"
#include "diskio.h"
#include "superblock.h"
#include "blockgroup.h"
#include "diskcow.h"

int disk_read_sector(e3tools_t *e3t, sector_t s, uint8_t *buf)
{
	/* XXX generalize as needed? */
	/* XXX this sucks */
	if (diskcow_read(e3t, s, buf))
		return 0;
	
	if (e3t->diskfd == -1)
		e3t->diskfd = open("recover"/*/dev/storage/storage0"*/, O_RDONLY);
	if (e3t->diskfd == -1)	/* Still? */
		return -1;	/* oh well */
	if (lseek64(e3t->diskfd, s * BYTES_PER_SECTOR, SEEK_SET) == (off64_t)-1)
		return -1;	/* oh well */
	if (read(e3t->diskfd, buf, BYTES_PER_SECTOR) < BYTES_PER_SECTOR)
		return -1;
	return 0;
}

int disk_read_block(e3tools_t *e3t, block_t b, uint8_t *buf)
{
	int i;
	sector_t s;
	
	s = ((sector_t)b) * ((sector_t)SB_BLOCK_SIZE(&e3t->sb) / BYTES_PER_SECTOR);
	for (i = 0; i < (SB_BLOCK_SIZE(&e3t->sb) / BYTES_PER_SECTOR); i++)
	{
		if (disk_read_sector(e3t, s, buf) < 0)
			return -1;
		s++;
		buf += BYTES_PER_SECTOR;
	}
	return 0;
}

int disk_write_sector(e3tools_t *e3t, sector_t s, uint8_t *buf)
{
	return diskcow_write(e3t, s, buf);
}

int disk_close(e3tools_t *e3t)
{
	if (e3t->diskfd >= 0)
		close(e3t->diskfd);
}
