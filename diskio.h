#ifndef __DISKIO_H
#define __DISKIO_H

#define _LARGEFILE64_SOURCE
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdint.h>
#include <unistd.h>

#define BYTES_PER_SECTOR 512LL

#include "e3tools.h"
#include "superblock.h"
#include "blockgroup.h"

typedef uint64_t sector_t;

extern int disk_read_sector(e3tools_t *e3t, sector_t s, uint8_t *buf);
extern int disk_read_block(e3tools_t *e3t, block_t b, uint8_t *buf);
extern int disk_write_sector(e3tools_t *e3t, sector_t s, uint8_t *buf);

#endif
