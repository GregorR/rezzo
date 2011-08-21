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

#ifndef AGENT_H
#define AGENT_H

#include <stdio.h>
#include <stdlib.h>

#include "buffer.h"
#include "ca.h"

#define VIEWPORT (13)
#define VIEWPORT_SQ (VIEWPORT*VIEWPORT)

typedef struct _Agent Agent;
typedef struct _AgentList AgentList;
typedef struct _ServerMessage ServerMessage;
typedef struct _ClientMessage ClientMessage;

enum ClientAcks {
    ACK_OK = 0,
    ACK_NO_MESSAGE,
    ACK_INVALID_MESSAGE,
    ACK_MULTIPLE_MESSAGES
};

enum ClientActions {
    ACT_FORWARD = '^',
    ACT_TURN_LEFT = '\\',
    ACT_TURN_RIGHT = '/',
    ACT_BUILD = '.',
    ACT_DESTROY = '-'
};

struct _Agent {
    Agent *next; /* agents form a list */
    unsigned char id; /* agent number */
    World *world; /* the world this agent is in */
    int x, y, c; /* location in it (must be consistent with map) and cardinality */
    unsigned char ts, ack; /* last turn sent to this client, and ack for its response (if any) */
    int rfd, wfd; /* FDs to read from and write to this agent */
    struct Buffer_char rbuf, wbuf; /* buffers for things to read/write */
};

struct _AgentList {
    Agent *head, *tail;
    World *world;
};

struct _ServerMessage {
    unsigned char ack, ts; /* acknowledgement of previous message, current timestamp */
    unsigned char c[VIEWPORT_SQ]; /* visible area */
    unsigned char damage[VIEWPORT_SQ]; /* damage in that area */
};

struct _ClientMessage {
    unsigned char ts, act; /* for the moment, the action is only one byte */
};

/* create an agent list */
AgentList *newAgentList(World *world);

/* generate a new client */
Agent *newAgent(AgentList *list, int rfd, int wfd);

/* generate a server message for this agent and buffer it */
void agentServerMessage(Agent *agent, unsigned char ts);

/* handle incoming data from this agent */
void agentIncoming(Agent *agent);

#endif
