LIBSOURCES = lib/diskio.c lib/diskcow.c lib/superblock.c lib/blockgroup.c lib/inode.c lib/e3tools.c
LIBOBJS = $(LIBSOURCES:.c=.o)

APPS = e3ls e3showsb e3showbgd e3repairbgd e3showitable e3checkitables e3showinode e3dumpblock

DEPFILES = $(LIBSOURCES:.c=.d) $(APPS:=.d)

CC = gcc
CFLAGS ?= -O2
CPPFLAGS += -Ilib -D__KERNEL_STRICT_NAMES

all: $(APPS)

%: %.o lib/libe3tools.a
	gcc -o $@ $< lib/libe3tools.a

lib/libe3tools.a: $(LIBOBJS)
	rm -f lib/libe3tools.a
	ar rcs lib/libe3tools.a $(LIBOBJS)

clean:
	rm -f $(LIBOBJS) $(APPS) $(APPS:=.o) $(DEPFILES)

%.d: %.c
	@$(CC) -M $(CPPFLAGS) $< | sed "s#$*.o#& $@#g" > $@

-include $(DEPFILES)
