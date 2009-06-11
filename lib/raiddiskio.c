/* raiddiskio.c */

#define _LARGEFILE64_SOURCE
#include <sys/types.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "raiddiskio.h"

#define RAID_DISKS 3
#define DATA_DISKS 2
#define CHUNK_SIZE 65536
#define SECTORS_PER_CHUNK (CHUNK_SIZE >> 9)
#define LVM_OFFSET 384

static diskio_t *_raiddisk_open(char *str)
{
	struct raiddiskio *rd;
	char tmp[16];
	int i;
	
	if (strcmp("raid:", str))
		return NULL;	/* Didn't match */
	
	rd = malloc(sizeof(*rd));
	if (!rd)
		return NULL;
	memcpy(&rd->ops, &raiddisk_ops, sizeof(diskio_t));
	
	for(i = 0; i < RAID_DISKS; i++)
	{
		snprintf(tmp, sizeof(tmp), "/dev/loop%d", i);
		rd->diskfd[i] = open(tmp, O_RDONLY);
		if (rd->diskfd[i] == -1)	/* Still? */
		{
			while(i--)
				close(rd->diskfd[i]);
			free(rd);
			return NULL;	/* oh well */
		}
	}
	
	return (diskio_t *)rd;
}

static int _raiddisk_read_sector(diskio_t *disk, sector_t s, uint8_t *buf)
{
	struct raiddiskio *rd = (struct raiddiskio *)disk;
	int pd_idx, dd_idx;
	long stripe;
	unsigned long chunk_number;
	unsigned int chunk_offset;
	sector_t new_sector;
	
	s += (sector_t)LVM_OFFSET;
	
	/* begin some hideousness that should probably be ripped out into
	 * another function, and heavily ganked from
	 * linux-source-$foo/drivers/md/raid5.c:raid5_compute_sector() */
	chunk_offset = s % SECTORS_PER_CHUNK;
	s /= SECTORS_PER_CHUNK;
	chunk_number = s;

	stripe = chunk_number / DATA_DISKS;
	dd_idx = chunk_number % DATA_DISKS;

	/* XXX ASLKDfja;lkfja;sdlkfjas;df 
	 * Somewhere, somehow, the relevant algorithm is stored on the disk
	 * along with other metadata (I assume). After fighting with linux 
	 * source for a while I am writing in the default case 
	 * (ALGORITHM_LEFT_SYMMETRIC). Joshua probably knows where to find 
	 * this data. Ideally the other cases would be plopped in here later. 
	 * drivers/md/md.c:mddev_find() may be of interest/use? */
	pd_idx = DATA_DISKS - stripe % RAID_DISKS;
	dd_idx = (pd_idx + 1 + dd_idx) % RAID_DISKS;
	
	new_sector = (sector_t)stripe * SECTORS_PER_CHUNK + chunk_offset;

	/* even this is uncertain! */
	if (lseek64(rd->diskfd[dd_idx], new_sector * BYTES_PER_SECTOR, SEEK_SET) == (off64_t)-1)
		return -1;	/* oh well */
	if (read(rd->diskfd[dd_idx], buf, BYTES_PER_SECTOR) < BYTES_PER_SECTOR)
		return -1;
	return 0;
}

static int _raiddisk_close(diskio_t *disk)
{
	struct raiddiskio *rd = (struct raiddiskio *)disk;
	int i;
	
	for (i = 0; i < RAID_DISKS; i++)
		if (rd->diskfd[i] >= 0)
			close(rd->diskfd[i]);
}

diskio_t raiddisk_ops = {
	.open = _raiddisk_open,
	.read_sector = _raiddisk_read_sector,
	.close = _raiddisk_close
};
