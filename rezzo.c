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

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#include <pthread.h>

#include "agent.h"
#include "buffer.h"
#include "ca.h"
#include "ui.h"

BUFFER(charp, char *);

/* global (YAY!) properties */
static int useLocks;
static int timeout, mustTimeout;

/* info for the agent thread */
typedef struct _AgentThreadData AgentThreadData;
struct _AgentThreadData {
    AgentList *agents;
    void *ui;
};

static char help_text[] =
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

void nonblocking(int fd)
{
    int flags, tmpi;
    SF(flags, fcntl, -1, (fd, F_GETFL, 0));
    SF(tmpi, fcntl, -1, (fd, F_SETFL, flags | O_NONBLOCK));
}

void *agentThread(void *datavp);
pthread_mutex_t bigLock;

int main(int argc, char **argv)
{
    int w, h, z, r, i, tmpi;
    struct timeval tv;
    void *uibuf;
    World *world;
    AgentList *agents;
    struct Buffer_charp agentProgs;
    pthread_t agentPThread;
    AgentThreadData atd;

    /* defaults */
    useLocks = 0;
    timeout = 60000;
    mustTimeout = 1;
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

    /* initialize the UI */
    uibuf = uiInit(agents, w, h, z);

    /* and the agent thread */
    atd.agents = agents;
    atd.ui = uibuf;
    pthread_mutex_init(&bigLock, NULL);
    pthread_create(&agentPThread, NULL, agentThread, &atd);

    /* then do the UI's loop */
    uiRun(agents, uibuf, z, useLocks ? &bigLock : NULL);

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

void *agentThread(void *data)
{
    AgentThreadData *atd = data;
    AgentList *agents = atd->agents;
    void *ui = atd->ui;
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
            tick(agents);
            uiQueueDraw(ui);

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
