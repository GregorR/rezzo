CC=gcc
CFLAGS=-g -O3
LDFLAGS=

UI=sdl

CLIBFLAGS=
LIBS=-pthread
ifeq ($(UI),sdl)
CLIBFLAGS+=`sdl-config --cflags` `pkg-config --cflags libpng`
LIBS+=`sdl-config --libs` `pkg-config --libs libpng`

else
ifeq ($(UI),headless)
CLIBFLAGS+=`pkg-config --cflags libpng`
LIBS+=`pkg-config --libs libpng`

endif
endif

OBJS=agent.o ca.o rezzo.o r$(UI).o

all: rezzo

rezzo: $(OBJS)
	$(CC) $(CFLAGS) $(CLIBFLAGS) $(LDFLAGS) $(OBJS) $(LIBS) -o rezzo

.SUFFIXES: .c .o

%.o: %.c
	$(CC) $(CFLAGS) $(CLIBFLAGS) -c $<

clean:
	rm -f *.o rezzo
	rm -f deps

-include deps

deps:
	$(CC) -MM *.c > deps || echo > deps
