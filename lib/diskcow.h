#ifndef _DISKCOW_H
#define _DISKCOW_H

struct exception;

#include "diskio.h"

extern int diskcow_import(e3tools_t *e3t, char *fname);
extern int diskcow_read(e3tools_t *e3t, sector_t s, uint8_t *buf);
extern int diskcow_write(e3tools_t *e3t, sector_t s, uint8_t *buf);
extern int diskcow_export(e3tools_t *e3t, char *fname);

#endif
