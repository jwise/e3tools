SOURCES = diskio.c e3view.c superblock.c
OBJS = $(SOURCES:.c=.o)
CC = gcc
CFLAGS ?= -O2

e3view: $(OBJS)
	gcc -o e3view $(OBJS)

clean:
	rm -f $(OBJS) e3view

%.d: %.c
	@$(CC) -M $(CPPFLAGS) $< | sed "s/$*.o/& $@/g" > $@

include $(SOURCES:.c=.d)
