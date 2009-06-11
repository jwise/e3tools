/* simplediskio.h */

#ifndef __RAIDDISKIO_H
#define __RAIDDISKIO_H

#include "diskio.h"

extern diskio_t raiddisk_ops;

struct raiddiskio {
	diskio_t ops;
	int diskfd[3];
};

#endif
