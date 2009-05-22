#ifndef __DISKIO_H
#define __DISKIO_H

#define _LARGEFILE64_SOURCE
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdint.h>
#include <unistd.h>

#define BYTES_PER_SECTOR 512LL

typedef uint64_t sector_t;

extern int read_sector(sector_t s, uint8_t *buf);
extern int write_sector(sector_t s, uint8_t *buf);

#endif
