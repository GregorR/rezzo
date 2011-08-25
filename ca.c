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

#define _BSD_SOURCE /* for random */

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

const CardinalityHelper cardinalityHelpers[] = {
    {1, 0, 0, 1},
    {0, 1, -1, 0},
    {-1, 0, 0, -1},
    {0, -1, 1, 0}
};

/* allocate a world */
World *newWorld(int w, int h)
{
    World *ret;

    /* allocate it */
    SF(ret, malloc, NULL, (sizeof(World)));
    ret->ts = 0;
    ret->losses[0] = 0;
    ret->w = w;
    ret->h = h;
    SF(ret->c, malloc, NULL, (w*h));
    memset(ret->c, CELL_NONE, w*h);
    SF(ret->c2, malloc, NULL, (w*h));
    SF(ret->owner, malloc, NULL, (w*h));
    memset(ret->owner, 0, w*h);
    SF(ret->o2, malloc, NULL, (w*h));
    SF(ret->damage, malloc, NULL, (w*h));
    memset(ret->damage, 0, w*h);

    return ret;
}

/* build an electron loop */
int buildLoop(World *world, int x, int y, int w, int h)
{
    int sx, sy;
    w += x;
    h += y;

    /* clear the substrate */
    for (sy = y-1; sy <= h; sy++) {
        for (sx = x-1; sx <= w; sx++) {
            world->c[getCell(world, sx, sy)] = CELL_NONE;
        }
    }

    /* build the loop */
    for (sx = x + 1; sx < w - 1; sx++) {
        world->c[getCell(world, sx, y)] = CELL_CONDUCTOR;
        world->c[getCell(world, sx, h-1)] = CELL_CONDUCTOR;
    }
    for (sy = y + 1; sy < h - 1; sy++) {
        world->c[getCell(world, x, sy)] = CELL_CONDUCTOR;
        world->c[getCell(world, w-1, sy)] = CELL_CONDUCTOR;
    }

    /* and the electron */
    world->c[getCell(world, x+1, y)] = CELL_ELECTRON;
    world->c[getCell(world, x, y+1)] = CELL_ELECTRON_TAIL;
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
        i = getCell(world, x, y);
        if ((dx == 0 && dy == 0) || world->c[i] != CELL_NONE) {
            world->c[i] = CELL_BASE;
            x = random() % (w/4) * 4;
            y = random() % (h/4) * 4;
            dx = dy = 0;
            while (!dx && !dy) {
                dx = (random() % 3) - 1;
                dy = (random() % 3) - 1;
            }
            d--;
            continue;
        }

        /* now draw here */
        world->c[i] = CELL_CONDUCTOR;

        x += dx;
        y += dy;
    }

    /* fix all the spots I forced not to be built */
    for (y = 0, dy = 0; y < h; y++, dy += w) {
        for (x = 0, i = dy; x < w; x++, i++) {
            if (world->c[i] == CELL_BASE)
                world->c[i] = CELL_NONE;
        }
    }

    /* then build the loops */
    tod = w*h/1024;
    if (tod == 0) tod = 1;
    for (d = 0; d < tod; d++) {
        x = random() % w;
        y = random() % h;
        dx = random() % 6 + 4;
        dy = random() % 6 + 4;
        buildLoop(world, x, y, dx, dy);
    }
}

/* get a cell id at a specified location, which may be out of bounds */
unsigned int getCell(World *world, int x, int y)
{
    while (x < 0) x += world->w;
    while (x >= world->w) x -= world->w;
    while (y < 0) y += world->h;
    while (y >= world->h) y -= world->h;
    return y*world->w+x;
}

/* mark a loss */
static void markLoss(World *world, unsigned char p)
{
    int i;
    for (i = 0; world->losses[i]; i++);
    world->losses[i] = p;
    world->losses[i+1] = 0;
}

/* update the cell in the middle of this neighborhood */
void updateCell(World *world, int x, int y, unsigned char *c, unsigned char *owner)
{
    int *neigh;
    unsigned char *ncs, self, sowner;
    int i, yi, xi;
    i = getCell(world, x, y);
    *c = self = world->c[i];
    *owner = sowner = world->owner[i];

    /* skip simple cases */
    switch (self) {
        /* complex cases: */
        case CELL_CONDUCTOR:
        case CELL_ELECTRON:
        case CELL_PHOTON:
        case CELL_FLAG:
            break;

        case CELL_ELECTRON_TAIL:
            *c = CELL_CONDUCTOR;
            return;

        default:
            return;
    }

    /* the rest all need a neighborhood */
    neigh = alloca(sizeof(int)*9);
    ncs = alloca(9);

    i = 0;
    for (yi = y - 1; yi <= y + 1; yi++) {
        for (xi = x - 1; xi <= x + 1; xi++, i++) {
            neigh[i] = getCell(world, xi, yi);
            ncs[i] = world->c[neigh[i]];
        }
    }

    if (self == CELL_CONDUCTOR) {
        /* check for electrons in the neighborhood */
        unsigned char electrons = 0;
        for (i = 0; i < 9; i++) {
            if (ncs[i] == CELL_ELECTRON) electrons++;
        }
        if (electrons == 1 || electrons == 2)
            *c = CELL_ELECTRON;

    } else if (self == CELL_ELECTRON) {
        /* check neighborhood for flags and tails */
        unsigned char flags = 0;
        unsigned char tails = 0;
        for (i = 0; i < 9; i++) {
            if (ncs[i] == CELL_FLAG || ncs[i] == CELL_FLAG_GEYSER) flags++;
            else if (ncs[i] == CELL_ELECTRON_TAIL) tails++;
        }
        if (flags && tails) {
            /* become a photon */
            *c = CELL_PHOTON;
        } else {
            /* just dissipate */
            *c = CELL_ELECTRON_TAIL;
        }

    } else if (self == CELL_PHOTON) {
        /* check neighborhood for flags */
        unsigned char newOwner = 0;
        for (i = 0; i < 9; i++) {
            if (ncs[i] == CELL_FLAG || ncs[i] == CELL_FLAG_GEYSER) {
                if (newOwner != 0 && world->owner[neigh[i]] != newOwner)
                    break;
                newOwner = world->owner[neigh[i]];
            }
        }
        if (i == 9 && newOwner != 0) {
            /* become a flag */
            *c = CELL_FLAG;
            *owner = newOwner;
        } else {
            /* just dissipate */
            *c = CELL_CONDUCTOR;
        }

    } else if (self == CELL_FLAG) {
        /* check for bases in the neighborhood (for losses) */
        for (i = 0; i < 9; i++) {
            if (ncs[i] == CELL_BASE && world->owner[neigh[i]] != sowner)
                markLoss(world, sowner);
        }

        /* check for photons in the neighborhood (for dissipation) */
        for (i = 0; i < 9; i++) {
            if (ncs[i] == CELL_PHOTON)
                break;
        }

        if (i < 9) {
            /* OK, we can dissipate */
            *c = CELL_CONDUCTOR;
            *owner = 0;
        }

    }
}

/* update the whole world */
void updateWorld(World *world, int iter)
{
    int x, y, yoff, i, w, h, wh;
    unsigned char *tmp;
    w = world->w;
    h = world->h;
    wh = w * h;

    while (iter--) {
        memcpy(world->c2, world->c, wh);
        memcpy(world->o2, world->owner, wh);
        for (y = 0, yoff = 0; y < h; y++, yoff += w) {
            for (x = 0, i = yoff; x < w; x++, i++) {
                updateCell(world, x, y, world->c2 + i, world->o2 + i);
            }
        }

        /* swap buffers */
        tmp = world->c;
        world->c = world->c2;
        world->c2 = tmp;

        tmp = world->owner;
        world->owner = world->o2;
        world->o2 = tmp;

        world->ts++;
    }
}

/* generate a viewport char for this location */
static unsigned char viewportChar(World *world, int i)
{
    char ret = world->c[i];
    if (ret == CELL_AGENT || ret == CELL_FLAG || ret == CELL_FLAG_GEYSER || ret == CELL_BASE)
        ret += world->owner[i] - 1;
    return ret;
}

/* generate a viewport from this location and cardinality */
void viewport(unsigned char *c, unsigned char *damage, World *world, int x, int y, int cardinality, int sz)
{
    CardinalityHelper ch;
    int sx, sy, hsz, i, cell;

    hsz = sz/2;

    /* crazy math time */
    ch = cardinalityHelpers[cardinality];

    /* now loop over the whole area */
    i = 0;
    for (sy = -sz + 1; sy <= 0; sy++) {
        for (sx = -hsz; sx <= hsz; sx++, i++) {
            cell = getCell(world,
                x + ch.xr*sx + ch.xd*sy,
                y + ch.yr*sx + ch.yd*sy);
            c[i] = viewportChar(world, cell);
            damage[i] = world->damage[cell];
        }
    }
}
