// e3tools disk COW layer
// Utility to make sense out of really damaged ext2/ext3 filesystems.
//
// If you have to make an assumption, write it down. Better assumptions may
// lead to better grades.

#include <string.h>
#include <stdint.h>

#include "e3tools.h"
#include "diskio.h"

struct exception {
	sector_t sector;
	uint8_t data[BYTES_PER_SECTOR];
	struct exception *next;
};

int diskcow_read(e3tools_t *e3t, sector_t s, uint8_t *buf)
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

int diskcow_write(e3tools_t *e3t, sector_t s, uint8_t *buf)
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

int diskcow_export(e3tools_t *e3t, char *fname)
{
	int i = 0;
	struct exception *exn;
	
	for (exn = e3t->exceptions; exn; exn = exn->next)
		i++;
	
	if (i > 0)
		printf("%d dirty sectors, comprising %lld bytes\n", i, i*BYTES_PER_SECTOR);
	
	return 0;
}
