#ifndef _PROTOCOLS_H
#define _PROTOCOLS_H

#define MAX_CONNECTIONS 32
#define MAX_MESSAGE 1500
#define MAX_TOPIC 50

enum client_command {
    SUBSCRIBE,
    UNSUBSCRIBE,
    EXIT,
};

struct udppkt {
    char topic[MAX_TOPIC];
    uint8_t type;
    char content[MAX_MESSAGE];
};

#endif