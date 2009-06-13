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

extern diskio_t raiddisk_ops, simpledisk_ops;

static diskio_t *mechanisms[] = {
        &raiddisk_ops,
	&simpledisk_ops,
	NULL
};

int disk_open(e3tools_t *e3t, char *desc)
{
	static diskio_t **mech;
	
	for (mech = mechanisms; *mech; mech++)
		if ((e3t->disk = (*mech)->open(desc)) != NULL)
			return 0;
	
	return -1;
}

int disk_read_sector(e3tools_t *e3t, sector_t s, uint8_t *buf)
{
	if (diskcow_read(e3t, s, buf))
		return 0;
	
	return e3t->disk->read_sector(e3t->disk, s, buf);
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
	return e3t->disk->close(e3t->disk);
}

int disk_lame_sector(e3tools_t *e3t, sector_t s)
{
	return e3t->disk->lame_sector(e3t->disk, s);
}
