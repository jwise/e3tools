OBJS = diskio.o e3view.o
CC = gcc
CFLAGS ?= -O2

e3view: $(OBJS)
	gcc -o e3view $(OBJS)

clean:
	rm -f $(OBJS) e3view
