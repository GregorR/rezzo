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
#include <unistd.h>

#include "agent.h"

/* create an agent list */
AgentList *newAgentList(World *world)
{
    AgentList *ret;
    SF(ret, malloc, NULL, (sizeof(AgentList)));
    memset(ret, 0, sizeof(AgentList));
    ret->world = world;
    return ret;
}

/* generate a new client */
Agent *newAgent(AgentList *list, int rfd, int wfd)
{
    World *world = list->world;
    Agent *ret;
    int i, posok, x, y;
    CardinalityHelper ch;

    SF(ret, malloc, NULL, (sizeof(Agent)));
    memset(ret, 0, sizeof(Agent));
    ret->world = list->world;
    ret->rfd = rfd;
    ret->wfd = wfd;

    INIT_BUFFER(ret->rbuf);
    INIT_BUFFER(ret->wbuf);

    if (!list->head) {
        list->head = list->tail = ret;
        ret->id = 1;
    } else {
        ret->id = list->tail->id + 1;
        list->tail->next = ret;
        list->tail = ret;
    }

    /* put it somewhere random */
    posok = 0;
    while (!posok) {
        ret->x = random() % world->w;
        ret->y = random() % world->h;
        i = getCell(world, ret->x, ret->y);

        /* check the surrounding area */
        posok = 1;
        for (y = -2; y <= 2; y++) {
            for (x = -2; x <= 2; x++) {
                if (world->owner[getCell(world, ret->x + x, ret->y + y)]) {
                    posok = 0;
                    goto posnotok;
                }
            }
        }
        posnotok: (void) 0;
    }
    ret->startx = ret->x;
    ret->starty = ret->y;

    /* reset the surrounding area to the proper configuration */
    for (y = -2; y <= 2; y++) {
        for (x = -2; x <= 2; x++) {
            world->c[getCell(world, ret->x + x, ret->y + y)] = CELL_NONE;
        }
    }
    ret->c = random() % CARDINALITIES;
    ch = cardinalityHelpers[ret->c];

    world->c[i] = CELL_AGENT;
    world->owner[i] = ret->id;

    /* base cells */
    i = getCell(world,
        ret->x + ch.xr*-1 + ch.xd*-1,
        ret->y + ch.yr*-1 + ch.yd*-1);
    world->c[i] = CELL_BASE;
    world->owner[i] = ret->id;
    i = getCell(world,
        ret->x + ch.xr*1 + ch.xd*-1,
        ret->y + ch.yr*1 + ch.yd*-1);
    world->c[i] = CELL_BASE;
    world->owner[i] = ret->id;
    i = getCell(world,
        ret->x + ch.xr*-1 + ch.xd*1,
        ret->y + ch.yr*-1 + ch.yd*1);
    world->c[i] = CELL_FLAG_GEYSER;
    world->owner[i] = ret->id;
    i = getCell(world,
        ret->x + ch.xr*1 + ch.xd*1,
        ret->y + ch.yr*1 + ch.yd*1);
    world->c[i] = CELL_FLAG_GEYSER;
    world->owner[i] = ret->id;

    return ret;
}

/* generate a server message for this agent and buffer it */
void agentServerMessage(Agent *agent)
{
    ServerMessage tosend;

    /* first the basics */
    tosend.ack = agent->ack;
    tosend.ts = agent->world->ts;
    agent->ts = tosend.ts;
    agent->ack = ACK_NO_MESSAGE;
    tosend.inv = agent->inv & 0xFF;

    /* then the viewport */
    viewport(tosend.c, tosend.damage, agent->world, agent->x, agent->y, agent->c, VIEWPORT);

    /* now add it to the queue */
    WRITE_BUFFER(agent->wbuf, &tosend, sizeof(ServerMessage));
}

static void agentClientMessage(Agent *agent, ClientMessage *cm);

/* handle incoming data from this agent */
void agentIncoming(Agent *agent)
{
    ssize_t rd;
    while (BUFFER_SPACE(agent->rbuf) < 1024) EXPAND_BUFFER(agent->rbuf);

    /* read some stuff */
    while ((rd = read(agent->rfd, BUFFER_END(agent->rbuf), BUFFER_SPACE(agent->rbuf))) > 0) {
        agent->rbuf.bufused += rd;
    }

    /* see if we have a command */
    while (agent->rbuf.bufused >= sizeof(ClientMessage)) {
        /* handle it */
        agentClientMessage(agent, (ClientMessage *) agent->rbuf.buf);

        /* then get rid of it */
        memmove(agent->rbuf.buf, agent->rbuf.buf + sizeof(ClientMessage), agent->rbuf.bufused - sizeof(ClientMessage));
        agent->rbuf.bufused -= sizeof(ClientMessage);
    }
}

static void agentClientMessage(Agent *agent, ClientMessage *cm)
{
    World *world = agent->world;
    unsigned char ack;
    int fx, fy, x, y, nx, ny, i, ni;

    if (cm->ts != agent->ts) {
        /* this isn't what I was expecting! */
        return;
    }

    if (agent->ack != ACK_NO_MESSAGE) {
        /* hey now, no repeats! */
        agent->ack = ACK_MULTIPLE_MESSAGES;
        return;
    }

    /* figure out our cardinality */
    fx = fy = 0;
    switch (agent->c) {
        case NORTH:
            fy = -1;
            break;

        case EAST:
            fx = 1;
            break;

        case SOUTH:
            fy = 1;
            break;

        case WEST:
            fx = -1;
            break;
    }

    /* and where that points to */
    x = agent->x;
    y = agent->y;
    i = getCell(world, x, y);
    nx = agent->x + fx;
    ny = agent->y + fy;
    if (nx < 0) nx += world->w;
    if (nx >= world->w) nx -= world->w;
    if (ny < 0) ny += world->h;
    if (ny >= world->h) ny -= world->h;
    ni = getCell(world, nx, ny);

    /* then perform the action */
    ack = ACK_OK;
    switch (cm->act) {
        case ACT_ADVANCE:
            if (world->c[ni] == CELL_NONE) {
                agent->x = nx;
                agent->y = ny;
                world->c[ni] = CELL_AGENT;
                world->owner[ni] = agent->id;
                world->damage[ni] = 0;
                world->c[i] = CELL_NONE;
                world->owner[i] = 0;
                world->damage[i] = 0;
            } else {
                ack = ACK_INVALID_ACTION;
            }
            break;

        case ACT_TURN_LEFT:
            agent->c--;
            if (agent->c < 0) agent->c += CARDINALITIES;
            break;

        case ACT_TURN_RIGHT:
            agent->c++;
            if (agent->c >= CARDINALITIES) agent->c -= CARDINALITIES;
            break;

        case ACT_BUILD:
            if (world->c[ni] == CELL_NONE) {
                agent->x = nx;
                agent->y = ny;
                world->c[ni] = CELL_AGENT;
                world->owner[ni] = agent->id;
                world->damage[ni] = 0;
                world->c[i] = CELL_CONDUCTOR;
                world->owner[i] = 0;
                world->damage[i] = 0;
            } else {
                ack = ACK_INVALID_ACTION;
            }
            break;

        case ACT_HIT:
            if (world->c[ni] == CELL_NONE || world->c[ni] == CELL_AGENT ||
                world->c[ni] == CELL_FLAG_GEYSER || world->c[ni] == CELL_BASE) {
                ack = ACK_INVALID_ACTION;
            } else {
                world->damage[ni]++;
                if (world->damage[ni] >= 4) {
                    /* DESTROY! EXTERMINATE! */
                    agent->inv++;
                    if (agent->inv < 0) agent->inv--;
                    world->c[ni] = CELL_NONE;
                    world->damage[ni] = 0;
                }
            }
            break;

        default:
            ack = ACK_INVALID_MESSAGE;
    }
    agent->ack = ack;
}
