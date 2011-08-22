CC=gcc
CFLAGS=-g -O3
CLIBFLAGS=`sdl-config --cflags` `pkg-config --cflags libpng`
LDFLAGS=
LIBS=`sdl-config --libs` `pkg-config --libs libpng`

OBJS=agent.o ca.o rezzo.o

all: rezzo

rezzo: $(OBJS)
	$(CC) $(CFLAGS) $(CLIBFLAGS) $(LDFLAGS) $(OBJS) $(LIBS) -o rezzo

.SUFFIXES: .c .o

%.o: %.c
	$(CC) $(CFLAGS) $(CLIBFLAGS) -c $<

clean:
	rm -f $(OBJS) rezzo
	rm -f deps

-include deps

deps:
	$(CC) -MM *.c > deps || echo > deps
