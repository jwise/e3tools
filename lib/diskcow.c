// e3tools disk COW layer
// Utility to make sense out of really damaged ext2/ext3 filesystems.
//
// If you have to make an assumption, write it down. Better assumptions may
// lead to better grades.

#include <string.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "e3tools.h"
#include "diskcow.h"
#include "diskio.h"

struct exception {
	sector_t sector;
	uint8_t data[BYTES_PER_SECTOR];
	struct exception *next;
};

int diskcow_import(e3tools_t *e3t, char *fname)
{
	struct exception *exn = NULL;
	struct exception rdexn;
	int fd;
	
	e3t->exceptions = NULL;
	
	fd = open(fname, O_RDONLY);
	if (fd < 0)
	{
		perror("diskcow_export: open");
		return -1;
	}
	
	while (read(fd, &rdexn, sizeof(rdexn) - sizeof(&rdexn)) > 0)
	{
		if (exn == NULL)
		{
			exn = e3t->exceptions = malloc(sizeof(*exn));
			if (!exn)
				return -1;
			exn->next = NULL;
		} else {
			exn->next = malloc(sizeof(*exn));
			if (!exn)
				return -1;
			exn = exn->next;
			exn->next = NULL;
		}
		
		memcpy(exn, &rdexn, sizeof(rdexn) - sizeof(&rdexn));
	}
	
	close(fd);
	
	return 0;
}

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
	int fd;
	struct exception *exn;
	
	for (exn = e3t->exceptions; exn; exn = exn->next)
		i++;
	
	if (i > 0)
		printf("%d dirty sectors, comprising %lld bytes\n", i, i*BYTES_PER_SECTOR);
	
	if (!fname)
		return 0;
	
	fd = open(fname, O_WRONLY | O_CREAT | O_TRUNC, 0666);
	if (fd < 0)
	{
		perror("diskcow_export: open");
		return -1;
	}
	
	for (exn = e3t->exceptions; exn; exn = exn->next)
		if (write(fd, exn, sizeof(*exn) - sizeof(exn)) < 0)
		{
			perror("diskcow_export: write");
			close(fd);
			return -1;
		}
	
	close(fd);
	
	return 0;
}
