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

Uint32 *typeColors, *ownerColors;
int timeout, mustTimeout;

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
                    pix[si+w*z*zy+zx] = typeColors[world->c[wi]];
                }
            }
        }
    }

    SDL_UpdateRect(buf, 0, 0, buf->w, buf->h);
}

void tick(AgentList *agents)
{
    World *world = agents->world;
    Agent *agent;

    /* update the world */
    updateWorld(world, 1);

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
    COL(ELECTRON_TAIL, 127, 127, 0);
    COL(AGENT, 255, 0, 255);
    COL(FLAG, 255, 255, 255);
    COL(BASE, 255, 255, 255);
#undef COL
}

void nonblocking(int fd)
{
    int flags, tmpi;
    SF(flags, fcntl, -1, (fd, F_GETFL, 0));
    SF(tmpi, fcntl, -1, (fd, F_SETFL, flags | O_NONBLOCK));
}

void *agentThread(void *agentsvp);
int wakeup[2];

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
    timeout = 60000;
    mustTimeout = 1;
    w = h = 640;
    z = 1;
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
        } else ARGN(-t) {
            timeout = atoi(nextarg) * 1000;
            i++;
        } else ARG(-q) {
            mustTimeout = 0;
        } else if (arg[0] != '-') {
            WRITE_ONE_BUFFER(agentProgs, arg);
        } else {
            fprintf(stderr, "%s?! What's this nonsense!\n", arg);
            exit(1);
        }
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
            exit(1);
        }

        /* close the ends we don't need */
        close(rpipe[1]);
        close(wpipe[0]);

        /* then make the agent */
        agentServerMessage(newAgent(agents, rpipe[0], wpipe[1]));
    }

    /* and the agent thread */
    pthread_create(&agentPThread, NULL, agentThread, agents);

    /* initialize SDL */
    if (SDL_Init(SDL_INIT_EVERYTHING) < 0) SDLERR;
    atexit(SDL_Quit);
    if ((buf = SDL_SetVideoMode(w*z, h*z, 32, SDL_SWSURFACE|SDL_DOUBLEBUF)) == NULL) SDLERR;

    initColors(buf);
    drawWorld(world, buf, z);

    while (SDL_WaitEvent(&ev)) {
        switch (ev.type) {
            case SDL_QUIT:
                exit(0);
                break;

            case SDL_USEREVENT:
                drawWorld(world, buf, z);
                break;
        }
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

    while (1) {
        fd_set rdset, wrset;

        /* figure out who needs what */
        FD_ZERO(&rdset);
        FD_ZERO(&wrset);
        for (agent = agents->head; agent; agent = agent->next) {
            FD_SET(agent->rfd, &rdset);
            if (agent->rfd >= nfds) nfds = agent->rfd + 1;
            if (agent->wbuf.bufused) {
                FD_SET(agent->wfd, &wrset);
                if (agent->wfd >= nfds) nfds = agent->wfd + 1;
            }
        }

        /* then select something */
        gettimeofday(&cur, NULL);
        tvsub(&tv, next, cur);
        if (tv.tv_sec >= 0) {
            SF(sr, select, -1, (nfds, &rdset, &wrset, NULL, &tv));
        } else {
            sr = 0;
        }

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

    return NULL;
}
