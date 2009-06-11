/* simplediskio.h */

#ifndef __SIMPLEDISKIO_H
#define __SIMPLEDISKIO_H

#define _LARGEFILE64_SOURCE
#include "diskio.h"

extern diskio_t simpledisk_ops;

struct simplediskio {
	diskio_t ops;
	int diskfd;
};

static diskio_t *_simpledisk_open(char *str);
static int _simpledisk_read_sector(diskio_t *disk, sector_t s, uint8_t *buf);
static int _simpledisk_close(diskio_t *disk);

#endif
