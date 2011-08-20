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

#include <SDL.h>

#include "ca.h"
#include "helpers.h"

#define SDLERR do { \
    fprintf(stderr, "%s\n", SDL_GetError()); \
    exit(1); \
} while (0)

Uint32 *typeColors, *ownerColors;

Uint32 clock(Uint32 interval, void *param)
{
    SDL_Event ev;

    ev.type = SDL_USEREVENT;
    ev.user.code = (int) 'f';
    ev.user.data1 = ev.user.data2 = NULL;
    SDL_PushEvent(&ev);

    return interval;
}

void drawWorld(World *world, SDL_Surface *buf, int z)
{
    int w, h, x, y, zx, zy, wyoff, syoff, wi, si;

    /* NOTE: assuming buf is 32-bit */
    Uint32 *pix = buf->pixels;

    w = world->w;
    h = world->h;
    for (y = 0, wyoff = 0, syoff = 0; y < h; y++, wyoff += w, syoff += w*z*z) {
        for (x = 0, wi = wyoff, si = syoff; x < w; x++, wi++, si += z) {
            for (zy = 0; zy < z; zy++) {
                for (zx = 0; zx < z; zx++) {
                    pix[si+w*z*zy+zx] = typeColors[world->d[wi].type];
                }
            }
        }
    }

    SDL_UpdateRect(buf, 0, 0, buf->w, buf->h);
}

void tick(World *world, SDL_Surface *buf, int z)
{
    updateWorld(world, 10);
    drawWorld(world, buf, z);
}

void initColors(SDL_Surface *buf)
{
    SDL_PixelFormat *fmt = buf->format;

    SF(typeColors, malloc, NULL, (sizeof(Uint32)*CELL_TYPE_COUNT));
    memset(typeColors, 0, sizeof(Uint32)*CELL_TYPE_COUNT);

#define COL(c, r, g, b) typeColors[CELL_ ## c] = SDL_MapRGB(fmt, r, g, b)
    COL(BOUNDARY, 0, 0, 0);
    COL(NONE, 0, 0, 0);
    COL(CONDUCTOR_POTENTIA, 64, 64, 64);
    COL(CONDUCTOR, 127, 127, 127);
    COL(ELECTRON, 255, 255, 0);
    COL(ELECTRON_TAIL, 127, 127, 0);
    COL(FLAG, 255, 255, 255);
    COL(FLAG_TAIL, 127, 127, 127);
#undef COL
}

int main(int argc, char **argv)
{
    int w, h, z;
    SDL_Surface *buf;
    SDL_Event ev;
    World *world;

    if (argc != 4) {
        fprintf(stderr, "Use: rezzo <w> <h> <z>\n");
        exit(1);
    }
    w = atoi(argv[1]);
    h = atoi(argv[2]);
    z = atoi(argv[3]);

    /* make our world */
    world = newWorld(w, h);
    randWorld(world);

    /* initialize SDL */
    if (SDL_Init(SDL_INIT_EVERYTHING) < 0) SDLERR;
    atexit(SDL_Quit);
    if ((buf = SDL_SetVideoMode(w*z, h*z, 32, SDL_HWSURFACE|SDL_DOUBLEBUF)) == NULL) SDLERR;

    initColors(buf);
    drawWorld(world, buf, z);

    SDL_AddTimer(60, clock, NULL);

    while (SDL_WaitEvent(&ev)) {
        switch (ev.type) {
            case SDL_QUIT:
                exit(0);
                break;

            case SDL_USEREVENT:
                tick(world, buf, z);
                break;
        }
    }

    return 0;
}
