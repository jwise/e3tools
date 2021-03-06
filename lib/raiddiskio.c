// e3tools naive RAID+LVM disk I/O layer
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

#define RAID_DISKS 3
#define DATA_DISKS 2
#define CHUNK_SIZE 65536
#define SECTORS_PER_CHUNK (CHUNK_SIZE >> 9)
#define LVM_OFFSET 384

typedef unsigned long chunk_t;

struct raiddiskio {
	diskio_t ops;
	int diskfd[3];
	chunk_t *lames;
	int nlames;
	int alloclames;
};

static diskio_t *_open(char *str);
static int _read_sector(diskio_t *disk, sector_t s, uint8_t *buf);
static int _close(diskio_t *disk);
static int _lame_sector(diskio_t *disk, sector_t s);
static int __is_lame(struct raiddiskio *rd, chunk_t c);

diskio_t raiddisk_ops = {
	.open = _open,
	.read_sector = _read_sector,
	.close = _close,
	.lame_sector = _lame_sector,
};

static diskio_t *_open(char *str)
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
	rd->lames = NULL;
	rd->nlames = 0;
	rd->alloclames = 0;
	
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

static void __compute_disklocs(struct raiddiskio *rd, sector_t log, sector_t *phys, int *pd_idx, int *dd_idx)
{
	/* Ganked from linux/drivers/md/raid5.c:raid5_compute_sector() */
	/* XXX need to look up actual RAID parameters from disk!
	 * currently assuming RAID_DISKS/DATA_DISKS, and the default
	 * algorithm of ALGORITHM_LEFT_SYMMETRIC.
	 * should look at drivers/md/md.c:mddev_find() ?
	 */
	long stripe;
	unsigned long chunk_number;
	unsigned int chunk_offset;
	int lame;
	
	chunk_offset = log % SECTORS_PER_CHUNK;
	chunk_number = log / SECTORS_PER_CHUNK;
	
	lame = __is_lame(rd, chunk_number);
	
	stripe = chunk_number / DATA_DISKS;
	*dd_idx = chunk_number % DATA_DISKS;

	*pd_idx = DATA_DISKS - stripe % RAID_DISKS;
	*dd_idx = (*pd_idx + 1 + *dd_idx) % RAID_DISKS;
	
	if (lame && (*dd_idx == 1))
		*dd_idx = 2;
	else if (lame && (*dd_idx == 2))
		*dd_idx = 1;
	
	if (phys)
		*phys = (sector_t)stripe * SECTORS_PER_CHUNK + chunk_offset;
}

static int _read_sector(diskio_t *disk, sector_t s, uint8_t *buf)
{
	struct raiddiskio *rd = (struct raiddiskio *)disk;
	int pd_idx, dd_idx;
	sector_t new_sector;
	
	s += (sector_t)LVM_OFFSET;
	
	__compute_disklocs(rd, s, &new_sector, &pd_idx, &dd_idx);

	if (lseek64(rd->diskfd[dd_idx], new_sector * BYTES_PER_SECTOR, SEEK_SET) == (off64_t)-1)
		return -1;	/* oh well */
	if (read(rd->diskfd[dd_idx], buf, BYTES_PER_SECTOR) < BYTES_PER_SECTOR)
		return -1;
	return 0;
}

static int __is_lame(struct raiddiskio *rd, chunk_t c)
{
	int i;
	for (i = 0; i < rd->nlames; i++)
		if (rd->lames[i] == c)
			return 1;
	return 0;
}

static int _lame_sector(diskio_t *disk, sector_t s)
{
	struct raiddiskio *rd = (struct raiddiskio *)disk;
	chunk_t chunk_number;
	int pd_idx, dd_idx;
	
	s += (sector_t)LVM_OFFSET;
	
	chunk_number = s / SECTORS_PER_CHUNK;
	
	if (__is_lame(rd, chunk_number))
	{
		E3DEBUG(E3TOOLS_PFX "raiddiskio: trying to mark sector %lld (chunk %d) lame, but it was already lame :( I think you may be hosed\n", s, chunk_number);
		return -1;
	}
	
	__compute_disklocs(rd, s, NULL, &pd_idx, &dd_idx);
	if (dd_idx != 1 && dd_idx != 2)
	{
		E3DEBUG(E3TOOLS_PFX "raiddiskio: trying to mark sector %lld (chunk %d) lame, but it wasn't on a lame disk :( I think you may be hosed\n", s, chunk_number);
		return -1;
	}
	
	if (rd->nlames == rd->alloclames)
	{
		if (rd->alloclames == 0)
			rd->alloclames = 1;
		else
			rd->alloclames *= 2;
		rd->lames = realloc(rd->lames, rd->alloclames * sizeof(chunk_t));
	}
	rd->lames[rd->nlames] = chunk_number;
	rd->nlames++;
	
	return 0;
}

static int _close(diskio_t *disk)
{
	struct raiddiskio *rd = (struct raiddiskio *)disk;
	int i;
	
	for (i = 0; i < RAID_DISKS; i++)
		if (rd->diskfd[i] >= 0)
			close(rd->diskfd[i]);
}
