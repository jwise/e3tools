// e3view disk I/O layer
// Utility to make sense out of really damaged ext2/ext3 filesystems.
//
// If you have to make an assumption, write it down. Better assumptions may
// lead to better grades.

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

#include "diskio.h"

struct exception {
	sector_t sector;
	uint8_t data[BYTES_PER_SECTOR];
	struct exception *next;
};

static struct exception *exns = NULL;

static int exception_lookup(sector_t s, uint8_t *buf)
{
	struct exception *exn;
	
	for (exn = exns; exn && (exn->sector < s); exn = exn->next)
		;
	
	if (exn && (exn->sector == s))
	{
		memcpy(buf, exn->data, BYTES_PER_SECTOR);
		return 1;
	}
	return 0;
}

int read_sector(sector_t s, uint8_t *buf)
{
	/* XXX generalize as needed? */
	/* XXX this sucks */
	static unsigned int fd = -1;
	
	if (exception_lookup(s, buf))
		return 0;
	
	if (fd == -1)
		fd = open("/dev/storage/storage0", O_RDONLY);
	if (fd == -1)	/* Still? */
		return -1;	/* oh well */
	if (lseek64(fd, s * BYTES_PER_SECTOR, SEEK_SET) == (off64_t)-1)
		return -1;	/* oh well */
	if (read(fd, buf, BYTES_PER_SECTOR) < BYTES_PER_SECTOR)
		return -1;
	return 0;
}

static void dirty_stats()
{
	int i = 0;
	struct exception *exn;
	
	for (exn = exns; exn; exn = exn->next)
		i++;
	printf("%d dirty sectors, comprising %d bytes\n", i, i*BYTES_PER_SECTOR);
}

int write_sector(sector_t s, uint8_t *buf)
{
	struct exception *exn, *exnp;
	
	if (!exns)
	{
		atexit(dirty_stats);
	}
	
	exn = malloc(sizeof(*exn));
	if (!exn)
		return -1;
	
	for (exnp = exns; exnp && (exnp->sector != s) && exnp->next && (exnp->next->sector < s); exnp = exnp->next)
		;
		
	if (!exns)
	{
		exns = exn;
		exn->next = NULL;
	} else if (exnp->sector == s) {
		free(exn);	// No linked list update needed.
		memcpy(exnp->data, buf, BYTES_PER_SECTOR);
		return 0;
	} else {
		exn->next = exnp->next;
		exnp->next = exn;
	}
	
	memcpy(exn->data, buf, BYTES_PER_SECTOR);
	exn->sector = s;

	return 0;
}