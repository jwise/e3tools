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

/* XXX this should go into another file */
static diskio_t *_simpledisk_open(char *str);
static int _simpledisk_read_sector(diskio_t *disk, sector_t s, uint8_t *buf);
static int _simpledisk_close(diskio_t *disk);

diskio_t simpledisk_ops = {
	.open = _simpledisk_open,
	.read_sector = _simpledisk_read_sector,
	.close = _simpledisk_close
};

struct simplediskio {
	diskio_t ops;
	int diskfd;
};

static diskio_t *_simpledisk_open(char *str)
{
	struct simplediskio *sd;
	
	if (strncmp("simple:", str, 7))
		return NULL;	/* Didn't match */
	str += 7;
	
	sd = malloc(sizeof(*sd));
	if (!sd)
		return NULL;
	
	memcpy(&sd->ops, &simpledisk_ops, sizeof(diskio_t));
	sd->diskfd = open(str, O_RDONLY);
	if (sd->diskfd == -1)
	{
		perror("simpledisk_open: open");
		free(sd);
		return NULL;
	}
	
	return (diskio_t *)sd;
}

static int _simpledisk_read_sector(diskio_t *disk, sector_t s, uint8_t *buf)
{
	struct simplediskio *sd = (struct simplediskio *)disk;
	if (lseek64(sd->diskfd, s * BYTES_PER_SECTOR, SEEK_SET) == (off64_t)-1)
		return -1;	/* oh well */
	if (read(sd->diskfd, buf, BYTES_PER_SECTOR) < BYTES_PER_SECTOR)
		return -1;
	return 0;
}

static int _simpledisk_close(diskio_t *disk)
{
	struct simplediskio *sd = (struct simplediskio *)disk;
	close(sd->diskfd);
}

/* XXX above should go into another file*/

static diskio_t *mechanisms[] = {
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
