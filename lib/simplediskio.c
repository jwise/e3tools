// e3tools "simple" disk I/O layer
// Utility to make sense out of really damaged ext2/ext3 filesystems.
//
// If you have to make an assumption, write it down. Better assumptions may
// lead to better grades.

#define _LARGEFILE64_SOURCE
#include <sys/types.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "diskio.h"

struct simplediskio {
	diskio_t ops;
	int diskfd;
};

static diskio_t *_open(char *str);
static int _read_sector(diskio_t *disk, sector_t s, uint8_t *buf);
static int _close(diskio_t *disk);
static int _lame_sector(diskio_t *disk, sector_t s);

diskio_t simpledisk_ops = {
	.open = _open,
	.read_sector = _read_sector,
	.close = _close,
	.lame_sector = _lame_sector,
};

static diskio_t *_open(char *str)
{
	struct simplediskio *sd;
	
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

static int _read_sector(diskio_t *disk, sector_t s, uint8_t *buf)
{
	struct simplediskio *sd = (struct simplediskio *)disk;
	if (lseek64(sd->diskfd, s * BYTES_PER_SECTOR, SEEK_SET) == (off64_t)-1)
		return -1;	/* oh well */
	if (read(sd->diskfd, buf, BYTES_PER_SECTOR) < BYTES_PER_SECTOR)
		return -1;
	return 0;
}

static int _close(diskio_t *disk)
{
	struct simplediskio *sd = (struct simplediskio *)disk;
	close(sd->diskfd);
	return 0;
}

static int _lame_sector(diskio_t *disk, sector_t s)
{
	return -1;
}
