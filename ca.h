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

typedef struct _Cell Cell;
typedef struct _World World;

struct _Cell {
    unsigned char type, owner, damage;
};

struct _World {
    int w, h;
    Cell d[1];
};

enum CellTypes {
    CELL_UNKNOWN = 0,
    CELL_BOUNDARY,
    CELL_NONE,
    CELL_CONDUCTOR_POTENTIA,
    CELL_CONDUCTOR,
    CELL_ELECTRON,
    CELL_ELECTRON_TAIL,
    CELL_FLAG,
    CELL_FLAG_TAIL,
    CELL_TYPE_COUNT
};

/* allocate a world */
World *newWorld(int w, int h);

/* randomize a world */
void randWorld(World *world);

/* update the cell in the middle of this neighborhood */
void updateCell(const Cell *top, const Cell *middle, const Cell *bottom, Cell *into);

/* update the whole world */
void updateWorld(World *world);

#endif
