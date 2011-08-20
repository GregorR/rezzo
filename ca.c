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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef __WIN32
#include <alloca.h>
#else
#include <malloc.h>
#endif

#include "ca.h"
#include "helpers.h"

/*
   case CELL_BOUNDARY:
   case CELL_NONE:
   case CELL_CONDUCTOR_POTENTIA:
   case CELL_CONDUCTOR:
   case CELL_ELECTRON:
   case CELL_ELECTRON_TAIL:
   case CELL_FLAG:
   case CELL_FLAG_TAIL:
*/

/* allocate a world */
World *newWorld(int w, int h)
{
    World *ret;
    size_t wsz;
    int x, y, yoff, i;

    /* allocate it */
    wsz = sizeof(World) + (sizeof(Cell)*(w*h)*2);
    SF(ret, malloc, NULL, (wsz));
    memset(ret, 0, wsz);
    ret->w = w;
    ret->h = h;

    /* set up the cells */
    for (y = 0, yoff = 0; y < h; y++, yoff += w) {
        for (x = 0, i = yoff; x < w; x++, i++) {
            if (x == 0 || x == w-1 || y == 0 || y == h-1) {
                ret->d[i].type = CELL_BOUNDARY;
            } else {
                ret->d[i].type = CELL_NONE;
            }
        }
    }

    return ret;
}

/* randomize a world */
void randWorld(World *world)
{
    int w, h, tod, d, x, y, i, dx, dy;
    w = world->w;
    h = world->h;
    tod = w*h/8;

    x = y = dx = dy = 0;

    /* first build the substrate */
    for (d = 0; d < tod; d++) {
        i = y*w+x;
        if (world->d[i].type != CELL_NONE) {
            if (world->d[i].type != CELL_BOUNDARY)
                world->d[i].type = CELL_NONE;
            x = random() % (w/4) * 4;
            y = random() % (h/4) * 4;
            dx = dy = 0;
            while ((!dx && !dy) || (dx && dy)) {
                dx = (random() % 3) - 1;
                dy = (random() % 3) - 1;
            }
            d--;
            continue;
        }

        /* now draw here */
        world->d[i].type = CELL_CONDUCTOR;

        x += dx;
        y += dy;
    }

    /* then add the loops */
    tod = w*h/1024;
    for (d = 0; d < tod; d++) {
        x = random() % (w-8) + 1;
        y = random() % (h-8) + 1;

        /* clear the area */
        for (dy = y; dy < y + 7; dy++) {
            for (dx = x; dx < x + 7; dx++) {
                world->d[dy*w+dx].type = CELL_NONE;
            }
        }

        /* draw the loop */
        world->d[(y+1)*w+x+2].type = CELL_ELECTRON;
        world->d[(y+1)*w+x+3].type = CELL_CONDUCTOR;
        world->d[(y+2)*w+x+1].type = CELL_ELECTRON_TAIL;
        world->d[(y+2)*w+x+4].type = CELL_CONDUCTOR;
        world->d[(y+3)*w+x+1].type = CELL_CONDUCTOR;
        world->d[(y+3)*w+x+4].type = CELL_ELECTRON_TAIL;
        world->d[(y+4)*w+x+2].type = CELL_CONDUCTOR;
        world->d[(y+4)*w+x+3].type = CELL_ELECTRON;
    }
}

/* update the cell in the middle of this neighborhood */
void updateCell(const Cell *top, const Cell *middle, const Cell *bottom, Cell *into)
{
    Cell *neighborhood, self;
    int i;
    *into = self = middle[1];

    /* skip simple cases */
    switch (self.type) {
        /* complex cases: */
        case CELL_CONDUCTOR_POTENTIA:
        case CELL_CONDUCTOR:
        case CELL_FLAG:
            break;

        /* simple cases: */
        case CELL_ELECTRON:
            into->type = CELL_ELECTRON_TAIL;
            return;

        case CELL_ELECTRON_TAIL:
            into->type = CELL_CONDUCTOR;
            return;

        case CELL_FLAG_TAIL:
            into->type = CELL_CONDUCTOR;
            into->owner = 0;
            return;

        default:
            return;
    }

    /* the rest all need a neighborhood */
    neighborhood = alloca(sizeof(Cell)*9);
    memcpy(neighborhood, top, sizeof(Cell)*3);
    memcpy(neighborhood+3, middle, sizeof(Cell)*3);
    memcpy(neighborhood+6, bottom, sizeof(Cell)*3);
    neighborhood[4].type = CELL_NONE;

    if (self.type == CELL_CONDUCTOR_POTENTIA || self.type == CELL_CONDUCTOR) {
        Cell neigh;
        int flags;

        /* check for electrons/flags in the neighborhood */
        neigh.type = neigh.owner = neigh.damage = 0;
        flags = 0;
        for (i = 0; i < 9; i++) {
            if (neighborhood[i].type == CELL_ELECTRON && !flags) {
                neigh.type = CELL_ELECTRON;
            }
            if (neighborhood[i].type == CELL_FLAG) {
                if (neigh.type == CELL_FLAG) {
                    if (neigh.owner != neighborhood[i].owner)
                        flags++;
                } else {
                    flags++;
                    neigh.type = CELL_FLAG;
                    neigh.owner = neighborhood[i].owner;
                }
            }
        }
        if (flags > 1) {
            fprintf(stderr, "Demotion!\n");
            neigh.type = CELL_ELECTRON;
            neigh.owner = 0;
        }

        if (neigh.type != 0) {
            if (self.type == CELL_CONDUCTOR_POTENTIA) {
                into->type = CELL_CONDUCTOR;
            } else {
                into->type = neigh.type;
                into->owner = neigh.owner;
            }
        }

    } else if (self.type == CELL_FLAG) {
        /* check for conductors in the neighborhood */
        for (i = 0; i < 9; i++) {
            if (neighborhood[i].type == CELL_CONDUCTOR)
                break;
        }

        if (i < 9)
            into->type = CELL_FLAG_TAIL;

    }
}

/* update the whole world */
void updateWorld(World *world)
{
    int x, y, yoff, i, w, h, wh;
    w = world->w;
    h = world->h;
    wh = w * h;

    for (y = 0, yoff = 0; y < h-1; y++, yoff += w) {
        for (x = 0, i = yoff; x < w-1; x++, i++) {
            if (x == 0 || x == w-1 || y == 0 || y == h-1) {
                /* just copy barriers */
                world->d[wh+i] = world->d[i];

            } else {
                /* real update */
                updateCell(world->d + i - w - 1, world->d + i - 1, world->d + i + w - 1, world->d + wh + i);

            }
        }
    }

    /* then copy the buf in place */
    memcpy(world->d, world->d + wh, sizeof(Cell) * wh);
}
