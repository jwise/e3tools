/* simplediskio.c */

#define _LARGEFILE64_SOURCE
#include <sys/types.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "simplediskio.h"

diskio_t simpledisk_ops = {
	.open = _simpledisk_open,
	.read_sector = _simpledisk_read_sector,
	.close = _simpledisk_close
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
