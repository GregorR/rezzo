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

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <png.h>
#include <pthread.h>
#include <rfb/rfb.h>

#ifndef __WIN32
#include <alloca.h>
#else
#include <malloc.h>
#endif

#include "agent.h"
#include "ca.h"
#include "helpers.h"
#include "ui.h"

typedef struct _VNCBuf VNCBuf;
struct _VNCBuf {
    unsigned int w, h;
    int wakeup[2];
    rfbScreenInfoPtr rfb;
};

/* global state */
char *video = NULL;
unsigned long frame = 0;
static unsigned char *typeColors[3];
static unsigned char *ownerColors[3];

static void drawSpot(World *world, void *bufvp, int x, int y, int z,
                     unsigned char r, unsigned char g, unsigned char b)
{
    VNCBuf *buf = bufvp;
    int zx, zy, i, yoff, zi;
    unsigned char *pix;
    unsigned char or, og, ob, nr, ng, nb;

    while (x < 0) x += world->w;
    while (x >= world->w) x -= world->w;
    while (y < 0) y += world->h;
    while (y >= world->h) y -= world->h;

    pix = (unsigned char *) buf->rfb->frameBuffer;
    i = y*z*buf->w + x*z;

    for (zy = 0, yoff = i; zy < z; zy++, yoff += buf->w*4) {
        for (zx = 0, zi = yoff; zx < z; zx++, zi += 4) {
            or = pix[zi];
            og = pix[zi+1];
            ob = pix[zi+2];
            nr = (((int) r)*2 + (int) or) / 3;
            ng = (((int) g)*2 + (int) og) / 3;
            nb = (((int) b)*2 + (int) ob) / 3;
            pix[zi] = nr;
            pix[zi+1] = ng;
            pix[zi+2] = nb;
        }
    }
}

void drawWorld(AgentList *agents, void *bufvp, int z)
{
    VNCBuf *buf = bufvp;
    World *world = agents->world;
    int w, h, x, y, zx, zy, wyoff, syoff, wi, si;
    unsigned char r, g, b;
    Agent *agent;
    unsigned char *pix = (unsigned char *) buf->rfb->frameBuffer;

    /* draw the substrate */
    w = world->w;
    h = world->h;
    for (y = 0, wyoff = 0, syoff = 0; y < h; y++, wyoff += w, syoff += w*z*z*4) {
        for (x = 0, wi = wyoff, si = syoff; x < w; x++, wi++, si += z*4) {
            if (world->c[wi] == CELL_FLAG) {
                r = ownerColors[0][world->owner[wi]];
                g = ownerColors[1][world->owner[wi]];
                b = ownerColors[2][world->owner[wi]];
            } else {
                r = typeColors[0][world->c[wi]];
                g = typeColors[1][world->c[wi]];
                b = typeColors[2][world->c[wi]];
            }
            for (zy = 0; zy < z; zy++) {
                for (zx = 0; zx < z; zx++) {
                    pix[si+w*z*zy*4+zx*4] = r;
                    pix[si+w*z*zy*4+zx*4+1] = g;
                    pix[si+w*z*zy*4+zx*4+2] = b;
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

    rfbMarkRectAsModified(buf->rfb, 0, 0, w*z, h*z);

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
        for (y = 0, x = 0; y < buf->h; y++, x += buf->w * z * 4) {
            rowptrs[y] = (png_byte *) pix + x;
        }
        png_set_rows(png, pngi, rowptrs);

        /* then write it out (FIXME: assuming little-endian, which needs to be swapped) */
        png_write_png(png, pngi, PNG_TRANSFORM_IDENTITY, NULL);

pngFail:
        if (png) png_destroy_write_struct(&png, pngi ? &pngi : NULL);
        fclose(fout);
    }
}

static void initColors()
{
    SF(typeColors[0], malloc, NULL, (256));
    memset(typeColors[0], 0, 256);
    SF(typeColors[1], malloc, NULL, (256));
    memset(typeColors[1], 0, 256);
    SF(typeColors[2], malloc, NULL, (256));
    memset(typeColors[2], 0, 256);

    SF(ownerColors[0], malloc, NULL, (256));
    memset(ownerColors[0], 0, 256);
    SF(ownerColors[1], malloc, NULL, (256));
    memset(ownerColors[1], 0, 256);
    SF(ownerColors[2], malloc, NULL, (256));
    memset(ownerColors[2], 0, 256);

#define TCOL(c, r, g, b) do { \
    typeColors[0][c] = r; \
    typeColors[1][c] = g; \
    typeColors[2][c] = b; \
} while (0)
#define ACOL(c, r, g, b) do { \
    ownerColors[0][c] = r; \
    ownerColors[1][c] = g; \
    ownerColors[2][c] = b; \
} while (0)
#include "colors.h"
#undef TCOL
#undef ACOL
}

void *uiInit(AgentList *agents, int w, int h, int z)
{
    VNCBuf *buf;
    int tmpi;

    /* get our data */
    SF(buf, malloc, NULL, (sizeof(VNCBuf)));
    buf->w = w;
    buf->h = h;
    SF(tmpi, pipe, -1, (buf->wakeup));

    /* then get RFB's data */
    buf->rfb = rfbGetScreen(NULL, NULL, w*z, h*z, 8, 3, 4);
    SF(buf->rfb->frameBuffer, malloc, NULL, (w*h*z*z*4));

    initColors();
    drawWorld(agents, buf, z);

    return (void *) buf;
}

void uiRun(AgentList *agents, void *bufvp, int z, pthread_mutex_t *lock)
{
    VNCBuf *buf = bufvp;
    char junk;

    rfbInitServer(buf->rfb);
    rfbRunEventLoop(buf->rfb, -1, TRUE);

    while (read(buf->wakeup[0], &junk, 1) == 1) {
        if (lock) pthread_mutex_lock(lock);
        drawWorld(agents, buf, z);
        if (lock) pthread_mutex_unlock(lock);
    }
}

void uiQueueDraw(void *bufvp)
{
    VNCBuf *buf = bufvp;
    ssize_t tmpi;
    SF(tmpi, write, -1, (buf->wakeup[1], "w", 1));
}
