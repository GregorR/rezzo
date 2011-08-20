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
   case CELL_FLAG_GUYSER:
*/

/* allocate a world */
World *newWorld(int w, int h)
{
    World *ret;
    int x, y, yoff, i;

    /* allocate it */
    SF(ret, malloc, NULL, (sizeof(World)));
    ret->w = w;
    ret->h = h;
    SF(ret->d, malloc, NULL, (sizeof(Cell)*w*h*2));
    memset(ret->d, 0, sizeof(Cell)*w*h*2);
    SF(ret->damage, malloc, NULL, (w*h));
    memset(ret->damage, 0, w*h);

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

/* build an electron loop */
int buildLoop(World *world, int x, int y, int w, int h)
{
    int sx, sy, yoff, i;
    w += x;
    h += y;

    if (x < 2 || y < 2 || w > world->w - 2 || h > world->h - 2) return 0;

    /* clear the substrate */
    for (sy = y-1, yoff = (y-1)*world->w; sy <= h; sy++, yoff += world->w) {
        for (sx = x-1, i = yoff+x-1; sx <= w; sx++, i++) {
            world->d[i].type = CELL_NONE;
        }
    }

    /* build the loop */
    for (sx = x + 1, i = y*world->w+x+1; sx < w - 1; sx++, i++)
        world->d[i].type = CELL_CONDUCTOR;
    for (sy = y + 1, i = (y+1)*world->w+x; sy < h - 1; sy++, i += world->w)
        world->d[i].type = CELL_CONDUCTOR;
    for (sy = y + 1, i = (y+1)*world->w+w-1; sy < h - 1; sy++, i += world->w)
        world->d[i].type = CELL_CONDUCTOR;
    for (sx = x + 1, i = (h-1)*world->w+x+1; sx < w - 1; sx++, i++)
        world->d[i].type = CELL_CONDUCTOR;

    /* and the electron */
    world->d[y*world->w+x+1].type = CELL_ELECTRON;
    world->d[(y+1)*world->w+x].type = CELL_ELECTRON_TAIL;
    return 1;
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
                world->d[i].type = CELL_UNKNOWN;
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

    /* fix all the left unknowns */
    for (y = 0, dy = 0; y < h; y++, dy += w) {
        for (x = 0, i = dy; x < w; x++, i++) {
            if (world->d[i].type == CELL_UNKNOWN)
                world->d[i].type = CELL_NONE;
        }
    }

    /* then build the loops */
    tod = w*h/1024;
    for (d = 0; d < tod; d++) {
        x = random() % w;
        y = random() % h;
        dx = random() % 6 + 4;
        dy = random() % 6 + 4;
        if (!buildLoop(world, x, y, dx, dy)) d--;
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
        case CELL_ELECTRON:
        case CELL_FLAG:
            break;

        case CELL_ELECTRON_TAIL:
            into->type = CELL_CONDUCTOR;
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
        /* check for electrons in the neighborhood */
        for (i = 0; i < 9; i++) {
            if (neighborhood[i].type == CELL_ELECTRON) break;
        }
        if (i < 9)
            into->type = self.type + 1;

    } else if (self.type == CELL_ELECTRON) {
        /* check neighborhood for flags */
        unsigned char owner = 0;
        for (i = 0; i < 9; i++) {
            if (neighborhood[i].type == CELL_FLAG) {
                if (owner != 0 && neighborhood[i].owner != owner)
                    break;
                owner = neighborhood[i].owner;
            }
        }
        if (i == 9 && owner != 0) {
            /* become a flag */
            into->type = CELL_FLAG;
            into->owner = owner;
        } else {
            /* just dissipate */
            into->type = CELL_ELECTRON_TAIL;
        }

    } else if (self.type == CELL_FLAG) {
        /* check for electrons in the neighborhood */
        for (i = 0; i < 9; i++) {
            if (neighborhood[i].type == CELL_ELECTRON)
                break;
        }

        if (i < 9) {
            /* OK, we can dissipate */
            into->type = CELL_CONDUCTOR;
            into->owner = 0;
        }

    }
}

/* update the whole world */
void updateWorld(World *world, int iter)
{
    int x, y, yoff, i, w, h, wh, prime, copybuf;
    w = world->w;
    h = world->h;
    wh = w * h;

    prime = 0;
    copybuf = wh;

    while (iter--) {
        for (y = 0, yoff = 0; y < h-1; y++, yoff += w) {
            for (x = 0, i = yoff; x < w-1; x++, i++) {
                if (x == 0 || x == w-1 || y == 0 || y == h-1) {
                    /* just copy barriers */
                    world->d[copybuf+i] = world->d[prime+i];

                } else {
                    /* real update */
                    updateCell(world->d + prime + i - w - 1,
                               world->d + prime + i - 1,
                               world->d + prime + i + w - 1,
                               world->d + copybuf + i);

                }
            }
        }

        if (prime == 0) {
            prime = wh;
            copybuf = 0;
        } else {
            prime = 0;
            copybuf = wh;
        }
    }

    /* then copy the buf in place */
    if (prime != 0)
        memcpy(world->d, world->d + wh, sizeof(Cell) * wh);
}
