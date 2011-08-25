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

else
ifeq ($(UI),vnc)
CLIBFLAGS+=`pkg-config --cflags libvncserver libpng`
LIBS+=`pkg-config --libs libvncserver libpng`

endif
endif
endif

# Interactive is always SDL
ICLIBFLAGS=`sdl-config --cflags`
ILIBS=`sdl-config --libs`

OBJS=agent.o ca.o rezzo.o r$(UI).o

all: rezzo

rezzo: $(OBJS)
	$(CC) $(CFLAGS) $(CLIBFLAGS) $(LDFLAGS) $(OBJS) $(LIBS) -o rezzo

interactive: interactive.c
	$(CC) $(CFLAGS) $(ICLIBFLAGS) $(LDFLAGS) interactive.c $(ILIBS) -o interactive

.SUFFIXES: .c .o

%.o: %.c
	$(CC) $(CFLAGS) $(CLIBFLAGS) -c $<

clean:
	rm -f *.o rezzo interactive
	rm -f deps

-include deps

deps:
	for i in *.c; do $(CC) -MM $(CLIBFLAGS) $$i; done > deps 2> /dev/null || echo > deps
