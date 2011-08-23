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

#define SDLERR do { \
    fprintf(stderr, "%s\n", SDL_GetError()); \
    exit(1); \
} while (0)

static Uint32 *typeColors, *ownerColors;
unsigned char clientAction = ACT_NOP;
unsigned char building = 0;
ServerMessage sm;

ssize_t readAll(int fd, char *buf, size_t count)
{
    ssize_t rd;
    size_t at;
    at = 0;
    while (at < count) {
        rd = read(fd, buf + at, count - at);
        if (rd <= 0) return -1;
        at += rd;
    }
    return at;
}

ssize_t writeAll(int fd, const char *buf, size_t count)
{
    ssize_t wr;
    size_t at;
    at = 0;
    while (at < count) {
        wr = write(fd, buf + at, count - at);
        if (wr <= 0) return -1;
        at += wr;
    }
    return at;
}

void drawViewport(SDL_Surface *buf, int z)
{
    int w, h, x, y, zx, zy, wyoff, syoff, wi, si;
    Uint32 color;
    unsigned char c;

    /* NOTE: assuming buf is 32-bit */
    Uint32 *pix = buf->pixels;

    /* draw the viewport */
    w = h = VIEWPORT;
    for (y = 0, wyoff = 0, syoff = 0; y < h; y++, wyoff += w, syoff += w*z*z) {
        for (x = 0, wi = wyoff, si = syoff; x < w; x++, wi++, si += z) {
            c = sm.c[wi];
#define GRP(nm) \
            if (c >= CELL_ ## nm && c < CELL_ ## nm + 10) { \
                color = ownerColors[c - CELL_ ## nm + 1]; \
            } else

            GRP(AGENT)
            GRP(FLAG)
            GRP(FLAG_GEYSER)
            GRP(BASE)
            {
                color = typeColors[c];
            }
            for (zy = 0; zy < z; zy++) {
                for (zx = 0; zx < z; zx++) {
                    pix[si+w*z*zy+zx] = color;
                }
            }
        }
    }

    SDL_UpdateRect(buf, 0, 0, buf->w, buf->h);
}

static void initColors(SDL_Surface *buf)
{
    SDL_PixelFormat *fmt = buf->format;

    SF(typeColors, malloc, NULL, (sizeof(Uint32)*256));
    memset(typeColors, 0, sizeof(Uint32)*256);
    SF(ownerColors, malloc, NULL, (sizeof(Uint32)*(MAX_AGENTS+1)));
    memset(typeColors, 0, sizeof(Uint32)*(MAX_AGENTS+1));

#define TCOL(c, r, g, b) typeColors[c] = SDL_MapRGB(fmt, r, g, b)
#define ACOL(c, r, g, b) do { \
    ownerColors[c] = SDL_MapRGB(fmt, r, g, b); \
} while (0)
#include "colors.h"
#undef TCOL
#undef ACOL
}

void *dataThread(void *ign);

int main()
{
    SDL_Surface *buf;
    SDL_Event ev;
    int z = 20;
    pthread_t th;

    if (SDL_Init(SDL_INIT_EVERYTHING) < 0) SDLERR;
    atexit(SDL_Quit);
    if ((buf = SDL_SetVideoMode(VIEWPORT*z, VIEWPORT*z, 32, SDL_SWSURFACE|SDL_DOUBLEBUF)) == NULL) SDLERR;
    SDL_WM_SetCaption("Rezzo Interactive", "Rezzo Interactive");

    initColors(buf);

    pthread_create(&th, NULL, dataThread, NULL);

    while (SDL_WaitEvent(&ev)) {
        switch (ev.type) {
            case SDL_QUIT:
                exit(0);
                break;

            case SDL_USEREVENT:
                drawViewport(buf, z);
                break;

            case SDL_KEYUP:
                if (ev.key.keysym.sym == SDLK_SPACE) {
                    building = 0;
                } else {
                    clientAction = ACT_NOP;
                }
                break;

            case SDL_KEYDOWN:
                switch (ev.key.keysym.sym) {
                    case SDLK_UP:
                        clientAction = ACT_ADVANCE;
                        break;

                    case SDLK_LEFT:
                        clientAction = ACT_TURN_LEFT;
                        break;

                    case SDLK_RIGHT:
                        clientAction = ACT_TURN_RIGHT;
                        break;

                    case SDLK_SPACE:
                        clientAction = ACT_BUILD;
                        building = 1;
                        break;

                    case SDLK_RETURN:
                        clientAction = ACT_HIT;
                        break;

                    default:
                        break;
                }
                break;
        }
    }

    return 0;
}

void uiQueueDraw()
{
    SDL_Event ev;

    ev.type = SDL_USEREVENT;
    ev.user.code = (int) 'f';
    ev.user.data1 = ev.user.data2 = NULL;

    SDL_PushEvent(&ev);
}

void *dataThread(void *ign)
{
    ClientMessage cm;

    while (readAll(0, (char *) &sm, sizeof(ServerMessage)) == sizeof(ServerMessage)) {
        uiQueueDraw();

        /* perform the requested client action */
        cm.ts = sm.ts;
        if (clientAction == ACT_ADVANCE && building) {
            cm.act = ACT_BUILD;
        } else {
            cm.act = clientAction;
        }
        writeAll(1, (char *) &cm, sizeof(ClientMessage));

        /* for certain client actions, don't repeat */
        if (clientAction == ACT_TURN_LEFT ||
            clientAction == ACT_TURN_RIGHT ||
            clientAction == ACT_BUILD)
            clientAction = ACT_NOP;
    }

    return NULL;
}
