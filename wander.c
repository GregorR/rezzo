#include <stdio.h>
#include <stdlib.h>

#include "agent.h"

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

int main()
{
    ServerMessage sm;
    ClientMessage cm;
    while (1) {
        readAll(0, (char *) &sm, sizeof(ServerMessage));
        cm.ts = sm.ts;
        if (sm.ack == ACK_INVALID_MESSAGE) {
            cm.act = ACT_TURN_RIGHT;
        } else {
            cm.act = ACT_FORWARD;
        }
        writeAll(1, (char *) &cm, sizeof(ClientMessage));
    }
    return 0;
}
