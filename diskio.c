// e3view disk I/O layer
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

#include "diskio.h"
#include "superblock.h"
#include "blockgroup.h"

struct exception {
	sector_t sector;
	uint8_t data[BYTES_PER_SECTOR];
	struct exception *next;
};

static int _exception_lookup(e3tools_t *e3t, sector_t s, uint8_t *buf)
{
	struct exception *exn;
	
	for (exn = e3t->exceptions; exn && (exn->sector < s); exn = exn->next)
		;
	
	if (exn && (exn->sector == s))
	{
		memcpy(buf, exn->data, BYTES_PER_SECTOR);
		return 1;
	}
	return 0;
}

int disk_read_sector(e3tools_t *e3t, sector_t s, uint8_t *buf)
{
	/* XXX generalize as needed? */
	/* XXX this sucks */
	unsigned int i;
        char tmp[16];
        int pd_idx, dd_idx;
        long stripe;
        unsigned long chunk_number;
        unsigned int chunk_offset;
        sector_t new_sector;
        
        /* XXX hurrrr (these should be derived elsewhere): */
        int chunk_size = 65536;
        int sectors_per_chunk = chunk_size >> 9;        /* ... ahahahaha no. */
        unsigned int raid_disks = 3;                  
        unsigned int data_disks = 2;                    /* is this true? */

	if (_exception_lookup(e3t, s, buf))
		return 0;

        /* open... */
	for(i = 0; i < raid_disks; i++)
	{
                snprintf(tmp, sizeof(tmp), "/dev/loop%d", i);
		if (e3t->diskfd[i] == -1)
                        e3t->diskfd[i] = open(tmp, O_RDONLY);
		if (e3t->diskfd[i] == -1)	/* Still? */
                {
			while(i--)
                                close(e3t->diskfd[i]);
                        return -1;	/* oh well */
                }
	}

        /* lvm offset */
	s += (sector_t)384;
        
        /* begin some hideousness that should probably be ripped out into
         * another function, and heavily ganked from
         * linux-source-$foo/drivers/md/raid5.c:raid5_compute_sector() */
        chunk_offset = s % sectors_per_chunk;
        s /= sectors_per_chunk;
        chunk_number = s;

        stripe = chunk_number / data_disks;
        dd_idx = chunk_number % data_disks;

        /* XXX ASLKDfja;lkfja;sdlkfjas;df 
         * Somewhere, somehow, the relevant algorithm is stored on the disk
         * along with other metadata (I assume). After fighting with linux 
         * source for a while I am writing in the default case 
         * (ALGORITHM_LEFT_SYMMETRIC). Joshua probably knows where to find 
         * this data. Ideally the other cases would be plopped in here later. 
         * drivers/md/md.c:mddev_find() may be of interest/use? */
        pd_idx = data_disks - stripe % raid_disks;
        dd_idx = (pd_idx + 1 + dd_idx) % raid_disks;
        
        new_sector = (sector_t)stripe * sectors_per_chunk + chunk_offset;

        /* even this is uncertain! */
        if (lseek64(e3t->diskfd[dd_idx], new_sector * BYTES_PER_SECTOR, SEEK_SET) == (off64_t)-1)
		return -1;	/* oh well */
	if (read(e3t->diskfd[dd_idx], buf, BYTES_PER_SECTOR) < BYTES_PER_SECTOR)
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
	struct exception *exn, *exnp;

	exn = malloc(sizeof(*exn));
	if (!exn)
		return -1;
	
	for (exnp = e3t->exceptions; exnp && (exnp->sector != s) && exnp->next && (exnp->next->sector < s); exnp = exnp->next)
		;
		
	if (!e3t->exceptions)
	{
		e3t->exceptions = exn;
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

int disk_close(e3tools_t *e3t)
{
	int i = 0, n;
	struct exception *exn;
	
	for (exn = e3t->exceptions; exn; exn = exn->next)
		i++;
	for (n = 0; n < 3; n++)
		if (e3t->diskfd[n] >= 0)
			close(e3t->diskfd[n]);
	if (i > 0)
		printf("%d dirty sectors, comprising %lld bytes\n", i, i*BYTES_PER_SECTOR);
}
