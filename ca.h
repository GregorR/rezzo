/*
 * Copyright (C) 2011 Gregor Richards
 * 
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef CA_H
#define CA_H

typedef struct _World World;

struct _World {
    unsigned char ts;
    int w, h;
    unsigned char *c, *c2, *owner, *o2, *damage;
};

enum CellTypes {
    CELL_NONE = ' ',
    CELL_CONDUCTOR = '.',
    CELL_ELECTRON = '*',
    CELL_ELECTRON_TAIL = '~',
    CELL_AGENT = '0',
    CELL_FLAG = 'A',
    CELL_BASE = 'a'
};

enum Cardinality {
    NORTH, EAST, SOUTH, WEST, CARDINALITIES
};

/* allocate a world */
World *newWorld(int w, int h);

/* randomize a world */
void randWorld(World *world);

/* get a cell id at a specified location, which may be out of bounds */
unsigned int getCell(World *world, int x, int y);

/* update the specified cell */
void updateCell(World *world, int x, int y, unsigned char *c, unsigned char *owner);

/* update the whole world */
void updateWorld(World *world, int iter);

/* generate a viewport from this location and cardinality */
void viewport(unsigned char *c, unsigned char *damage, World *world, int x, int y, int cardinality, int sz);

#endif
