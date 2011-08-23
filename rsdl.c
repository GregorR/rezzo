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

#include <png.h>
#include <SDL.h>

#include <pthread.h>

#include "agent.h"
#include "ca.h"
#include "ui.h"

#define SDLERR do { \
    fprintf(stderr, "%s\n", SDL_GetError()); \
    exit(1); \
} while (0)

char *video = NULL;
unsigned long frame = 0;
static Uint32 *typeColors, *ownerColors32;
static unsigned char *ownerColors[3];

static void drawSpot(World *world, void *bufvp, int x, int y, int z,
                     unsigned char r, unsigned char g, unsigned char b)
{
    SDL_Surface *buf = bufvp;
    SDL_PixelFormat *fmt = buf->format;
    int zx, zy, i, yoff, zi;
    Uint32 *pix;
    unsigned char or, og, ob, nr, ng, nb;

    while (x < 0) x += world->w;
    while (x >= world->w) x -= world->w;
    while (y < 0) y += world->h;
    while (y >= world->h) y -= world->h;

    i = y*z*buf->w + x*z;
    pix = buf->pixels;

    for (zy = 0, yoff = i; zy < z; zy++, yoff += buf->w) {
        for (zx = 0, zi = yoff; zx < z; zx++, zi++) {
            SDL_GetRGB(pix[zi], fmt, &or, &og, &ob);
            nr = (((int) r)*2 + (int) or) / 3;
            ng = (((int) g)*2 + (int) og) / 3;
            nb = (((int) b)*2 + (int) ob) / 3;
            pix[zi] = SDL_MapRGB(fmt, nr, ng, nb);
        }
    }
}

void drawWorld(AgentList *agents, void *bufvp, int z)
{
    SDL_Surface *buf = bufvp;
    World *world = agents->world;
    int w, h, x, y, zx, zy, wyoff, syoff, wi, si;
    Uint32 color;
    unsigned char r, g, b;
    Agent *agent;

    /* NOTE: assuming buf is 32-bit */
    Uint32 *pix = buf->pixels;

    /* draw the substrate */
    w = world->w;
    h = world->h;
    for (y = 0, wyoff = 0, syoff = 0; y < h; y++, wyoff += w, syoff += w*z*z) {
        for (x = 0, wi = wyoff, si = syoff; x < w; x++, wi++, si += z) {
            if (world->c[wi] == CELL_FLAG) {
                color = ownerColors32[world->owner[wi]];
            } else {
                color = typeColors[world->c[wi]];
            }
            for (zy = 0; zy < z; zy++) {
                for (zx = 0; zx < z; zx++) {
                    pix[si+w*z*zy+zx] = color;
                }
            }
        }
    }

    /* now draw the agents */
    for (agent = agents->head; agent; agent = agent->next) {
        CardinalityHelper ch = cardinalityHelpers[agent->c];
        if (!agent->alive) continue;
        r = ownerColors[0][agent->id];
        g = ownerColors[1][agent->id];
        b = ownerColors[2][agent->id];

        /* draw the arrow at the agent's location */
        drawSpot(world, buf, agent->x, agent->y, z, r, g, b);
        for (y = 1; y <= 2; y++) {
            for (x = -y; x <= y; x++) {
                drawSpot(world, buf,
                    agent->x + ch.xr*x + ch.xd*y,
                    agent->y + ch.yr*x + ch.yd*y,
                    z, r, g, b);
            }
        }

        /* and at the agent's base */
        for (y = agent->starty - 3; y <= agent->starty + 3; y++) {
            for (x = agent->startx - 3; x <= agent->startx + 3; x++) {
                drawSpot(world, buf, x, y, z, r, g, b);
            }
        }
    }

    SDL_UpdateRect(buf, 0, 0, buf->w, buf->h);

    /* maybe write it out */
    if (video) {
        FILE *fout = NULL;
        png_structp png = NULL;
        png_infop pngi = NULL;
        static char *fnm = NULL;
        png_byte **rowptrs = NULL;
        frame++;

        /* open the file */
        if (fnm == NULL)
            SF(fnm, malloc, NULL, (strlen(video) + 128));
        sprintf(fnm, "%s/%08lu.png", video, frame);
        SF(fout, fopen, NULL, (fnm, "wb"));

        /* prepare PNG */
        png = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
        if (!png) goto pngFail;
        pngi = png_create_info_struct(png);
        if (!pngi) goto pngFail;
        if (setjmp(png_jmpbuf(png))) goto pngFail;
        png_init_io(png, fout);

        png_set_IHDR(png, pngi, buf->w, buf->h, 8, PNG_COLOR_TYPE_RGB_ALPHA,
            PNG_INTERLACE_ADAM7, PNG_COMPRESSION_TYPE_DEFAULT,
            PNG_FILTER_TYPE_DEFAULT);

        /* set up the row pointers */
        rowptrs = alloca(buf->h * sizeof(png_byte *));
        for (y = 0, x = 0; y < buf->h; y++, x += buf->pitch) {
            rowptrs[y] = (png_byte *) pix + x;
        }
        png_set_rows(png, pngi, rowptrs);

        /* then write it out (FIXME: assuming little-endian, which needs to be swapped) */
        png_write_png(png, pngi, PNG_TRANSFORM_BGR, NULL);

pngFail:
        if (png) png_destroy_write_struct(&png, pngi ? &pngi : NULL);
        fclose(fout);
    }
}

static void initColors(SDL_Surface *buf)
{
    SDL_PixelFormat *fmt = buf->format;

    SF(typeColors, malloc, NULL, (sizeof(Uint32)*256));
    memset(typeColors, 0, sizeof(Uint32)*256);

#define COL(c, r, g, b) typeColors[CELL_ ## c] = SDL_MapRGB(fmt, r, g, b)
    COL(NONE, 0, 0, 0);
    COL(CONDUCTOR, 127, 127, 127);
    COL(ELECTRON, 255, 255, 0);
    COL(ELECTRON_TAIL, 64, 64, 0);

    /* these will all be colored by the player-coloring step */
    COL(AGENT, 255, 255, 255);
    COL(FLAG_GEYSER, 255, 255, 255);
    COL(BASE, 255, 255, 255);
#undef COL

    SF(ownerColors[0], malloc, NULL, (MAX_AGENTS+1));
    SF(ownerColors[1], malloc, NULL, (MAX_AGENTS+1));
    SF(ownerColors[2], malloc, NULL, (MAX_AGENTS+1));
    SF(ownerColors32, malloc, NULL, (sizeof(Uint32)*(MAX_AGENTS+1)));

#define COL(c, r, g, b) do { \
    ownerColors[0][c] = r; \
    ownerColors[1][c] = g; \
    ownerColors[2][c] = b; \
    ownerColors32[c] = SDL_MapRGB(fmt, r, g, b); \
} while (0)
    COL(1, 255, 0, 0); /* red */
    COL(2, 64, 64, 255); /* blue */
    COL(3, 0, 255, 0); /* green */
    COL(4, 255, 127, 0); /* orange */
    COL(5, 255, 0, 255); /* magenta */
    COL(6, 0, 255, 255); /* cyan */
    COL(7, 163, 109, 122); /* puce */
    COL(8, 0, 102, 102); /* teal */
    COL(9, 102, 0, 102); /* purple */
    COL(10, 255, 255, 255); /* grey (bleh) */
#undef COL
}

void *uiInit(AgentList *agents, int w, int h, int z)
{
    SDL_Surface *buf;

    if (SDL_Init(SDL_INIT_EVERYTHING) < 0) SDLERR;
    atexit(SDL_Quit);
    if ((buf = SDL_SetVideoMode(w*z, h*z, 32, SDL_SWSURFACE|SDL_DOUBLEBUF)) == NULL) SDLERR;
    SDL_WM_SetCaption("Rezzo", "Rezzo");

    initColors(buf);
    drawWorld(agents, buf, z);

    return (void *) buf;
}

void uiRun(AgentList *agents, void *bufvp, int z, pthread_mutex_t *lock)
{
    SDL_Surface *buf = bufvp;
    SDL_Event ev;

    while (SDL_WaitEvent(&ev)) {
        if (lock) pthread_mutex_lock(lock);
        switch (ev.type) {
            case SDL_QUIT:
                exit(0);
                break;

            case SDL_USEREVENT:
                drawWorld(agents, buf, z);
                break;
        }
        if (lock) pthread_mutex_unlock(lock);
    }
}

void uiQueueDraw()
{
    SDL_Event ev;

    ev.type = SDL_USEREVENT;
    ev.user.code = (int) 'f';
    ev.user.data1 = ev.user.data2 = NULL;

    SDL_PushEvent(&ev);
}
