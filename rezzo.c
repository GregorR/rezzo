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

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/select.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#include <png.h>
#include <pthread.h>
#include <SDL.h>

#include "agent.h"
#include "buffer.h"
#include "ca.h"

BUFFER(charp, char *);

#define SDLERR do { \
    fprintf(stderr, "%s\n", SDL_GetError()); \
    exit(1); \
} while (0)

Uint32 *typeColors, *ownerColors32;
unsigned char *ownerColors[3];
int useLocks;
char *video;
int timeout, mustTimeout;
unsigned long frame;

char help_text[] =
    "Usage: rezzo [options] warrior ...\n"
    "Options:\n"
    "\t-w N, -h N   Set arena size\n"
    "\t-z N         Set display zoom\n"
    "\t-t N         Set turn timeout\n"
    "\t-q           Advance to the next turn immediately if all players have\n"
    "\t             moved (quick mode)\n"
    "\t-r N         Set random seed\n"
    "\t-v <dir>     Output a \"video\" (sequence of PPM files) to the given\n"
    "\t             directory\n";

void drawSpot(World *world, SDL_Surface *buf, int x, int y, int z,
              unsigned char r, unsigned char g, unsigned char b)
{
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

void drawWorld(AgentList *agents, SDL_Surface *buf, int z)
{
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

void tick(AgentList *agents)
{
    World *world = agents->world;
    Agent *agent;

    /* update the world */
    updateWorld(world, 1);

    /* check for losses */
    agentProcessLosses(agents);

    /* tell the agents */
    for (agent = agents->head; agent; agent = agent->next) {
        agentServerMessage(agent);
    }
}

void initColors(SDL_Surface *buf)
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

void nonblocking(int fd)
{
    int flags, tmpi;
    SF(flags, fcntl, -1, (fd, F_GETFL, 0));
    SF(tmpi, fcntl, -1, (fd, F_SETFL, flags | O_NONBLOCK));
}

void *agentThread(void *agentsvp);
pthread_mutex_t bigLock;

int main(int argc, char **argv)
{
    int w, h, z, r, i, tmpi;
    struct timeval tv;
    SDL_Surface *buf;
    SDL_Event ev;
    World *world;
    AgentList *agents;
    struct Buffer_charp agentProgs;
    pthread_t agentPThread;

    /* defaults */
    useLocks = 0;
    video = NULL;
    timeout = 60000;
    mustTimeout = 1;
    frame = 0;
    w = h = 320;
    z = 2;
    gettimeofday(&tv, NULL);
    r = tv.tv_sec ^ tv.tv_usec ^ getpid();
    srandom(r);
    r = random();
    INIT_BUFFER(agentProgs);

    /* options */
    for (i = 1; i < argc; i++) {
        char *arg = argv[i];
        char *nextarg = (i < argc - 1) ? argv[i+1] : NULL;
#define ARG(x) if (!strcmp(arg, #x))
#define ARGN(x) if (!strcmp(arg, #x) && nextarg)
        ARGN(-w) {
            w = atoi(nextarg);
            i++;
        } else ARGN(-h) {
            h = atoi(nextarg);
            i++;
        } else ARGN(-z) {
            z = atoi(nextarg);
            i++;
        } else ARGN(-r) {
            r = atoi(nextarg);
            i++;
        } else ARGN(-v) {
            useLocks = 1;
            video = nextarg;
            i++;
        } else ARG(-l) {
            /* shhhh secret option */
            useLocks = 1;
        } else ARGN(-t) {
            timeout = atoi(nextarg) * 1000;
            i++;
        } else ARG(-q) {
            mustTimeout = 0;
        } else if (arg[0] != '-') {
            WRITE_ONE_BUFFER(agentProgs, arg);
        } else {
            fprintf(stderr, "Unknown option: %s\n%s", arg, help_text);
            exit(1);
        }
    }

    if (agentProgs.bufused > MAX_AGENTS) {
        fprintf(stderr, "No more than %d agents are allowed.\n", MAX_AGENTS);
        exit(1);
    }

    fprintf(stderr, "Random seed: %d\n", r);
    srandom(r);

    /* make our world */
    world = newWorld(w, h);
    randWorld(world);

    /* prepare our agents */
    agents = newAgentList(world);
    for (i = 0; i < agentProgs.bufused; i++) {
        char *prog = agentProgs.buf[i];
        int rpipe[2], wpipe[2];
        pid_t pid;

        /* prepare our pipes */
        SF(tmpi, pipe, -1, (rpipe));
        SF(tmpi, pipe, -1, (wpipe));
        nonblocking(rpipe[0]);
        nonblocking(wpipe[1]);

        /* then fork off */
        SF(pid, fork, -1, ());
        if (pid == 0) {
            int maxfd;
            dup2(rpipe[1], 1);
            dup2(wpipe[0], 0);

            /* close all other FDs */
            maxfd = sysconf(_SC_OPEN_MAX);
            for (i = 3; i < maxfd; i++) close(i);

            /* then go */
            execl(prog, prog, NULL);
            perror(prog);
            exit(1);
        }

        /* close the ends we don't need */
        close(rpipe[1]);
        close(wpipe[0]);

        /* then make the agent */
        agentServerMessage(newAgent(agents, pid, rpipe[0], wpipe[1]));
    }

    /* and the agent thread */
    pthread_mutex_init(&bigLock, NULL);
    pthread_create(&agentPThread, NULL, agentThread, agents);

    /* initialize SDL */
    if (SDL_Init(SDL_INIT_EVERYTHING) < 0) SDLERR;
    atexit(SDL_Quit);
    if ((buf = SDL_SetVideoMode(w*z, h*z, 32, SDL_SWSURFACE|SDL_DOUBLEBUF)) == NULL) SDLERR;
    SDL_WM_SetCaption("Rezzo", "Rezzo");

    initColors(buf);
    drawWorld(agents, buf, z);

    while (SDL_WaitEvent(&ev)) {
        if (useLocks) pthread_mutex_lock(&bigLock);
        switch (ev.type) {
            case SDL_QUIT:
                exit(0);
                break;

            case SDL_USEREVENT:
                drawWorld(agents, buf, z);
                break;
        }
        if (useLocks) pthread_mutex_unlock(&bigLock);
    }

    return 0;
}

static void tvsub(struct timeval *into, struct timeval a, struct timeval b)
{
    into->tv_sec = a.tv_sec - b.tv_sec;
    if (a.tv_usec < b.tv_usec) {
        into->tv_sec--;
        into->tv_usec = 1000000 - b.tv_usec + a.tv_usec;
    } else {
        into->tv_usec = a.tv_usec - b.tv_usec;
    }
}

static void tvadd(struct timeval *into, struct timeval a, long usec)
{
    into->tv_sec = a.tv_sec;
    into->tv_usec = a.tv_usec + usec;
    while (into->tv_usec >= 1000000) {
        into->tv_sec++;
        into->tv_usec -= 1000000;
    }
}

void *agentThread(void *agentsvp)
{
    AgentList *agents = agentsvp;
    Agent *agent;
    int nfds = 0, allDone, sr;
    struct timeval cur, next, tv;

    gettimeofday(&cur, NULL);
    tvadd(&next, cur, timeout);

    if (useLocks) pthread_mutex_lock(&bigLock);
    while (1) {
        fd_set rdset, wrset;

        /* figure out who needs what */
        FD_ZERO(&rdset);
        FD_ZERO(&wrset);
        for (agent = agents->head; agent; agent = agent->next) {
            if (agent->alive) {
                FD_SET(agent->rfd, &rdset);
                if (agent->rfd >= nfds) nfds = agent->rfd + 1;
                if (agent->wbuf.bufused) {
                    FD_SET(agent->wfd, &wrset);
                    if (agent->wfd >= nfds) nfds = agent->wfd + 1;
                }
            }
        }

        /* then select something */
        if (useLocks) pthread_mutex_unlock(&bigLock);
        gettimeofday(&cur, NULL);
        tvsub(&tv, next, cur);
        if (tv.tv_sec >= 0) {
            SF(sr, select, -1, (nfds, &rdset, &wrset, NULL, &tv));
        } else {
            sr = 0;
        }
        if (useLocks) pthread_mutex_lock(&bigLock);

        /* figure out which have read actions */
        allDone = 1;
        if (sr > 0) {
            if (mustTimeout) allDone = 0;
            for (agent = agents->head; agent; agent = agent->next) {
                if (FD_ISSET(agent->rfd, &rdset))
                    agentIncoming(agent);
                if (agent->ack == ACK_NO_MESSAGE)
                    allDone = 0;
            }
        }

        /* and maybe do a world step */
        if (allDone) {
            SDL_Event ev;

            ev.type = SDL_USEREVENT;
            ev.user.code = (int) 'f';
            ev.user.data1 = ev.user.data2 = NULL;

            tick(agents);
            SDL_PushEvent(&ev);

            /* how shall we proceed? */
            if (sr > 0) {
                /* everybody responded */
                tvadd(&next, cur, timeout);
            } else {
                /* timed out */
                tvadd(&next, next, timeout);
            }
        }

        /* then write actions */
        if (sr > 0) {
            for (agent = agents->head; agent; agent = agent->next) {
                if (FD_ISSET(agent->wfd, &wrset)) {
                    ssize_t wr;
                    wr = write(agent->wfd, agent->wbuf.buf, agent->wbuf.bufused);
                    if (wr <= 0) {
                        /* yukk! */
                        fprintf(stderr, "Failed to write to agent %d!\n", (int) agent->id);
                        agent->wbuf.bufused = 0;
                    } else {
                        memmove(agent->wbuf.buf, agent->wbuf.buf + wr, agent->wbuf.bufused - wr);
                        agent->wbuf.bufused -= wr;
                    }
                }
            }
        }
    }
    if (useLocks) pthread_mutex_unlock(&bigLock);

    return NULL;
}
