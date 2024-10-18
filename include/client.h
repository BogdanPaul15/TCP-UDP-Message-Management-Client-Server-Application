#ifndef _CLIENT_H
#define _CLIENT_H

#define ID_LEN 10
#define IP_LEN 16

enum client_state {
    DISCONNECTED,
    CONNECTED,
};

struct topic {
    char name[51];
};

struct client {
    char id[ID_LEN];
    int sockfd;
    enum client_state state;
    struct topic *topics;
    int ntopics;
};

struct client *get_client_by_id(char *id, struct client *clients, int nclients);
struct client *get_client_by_sockfd(int sockfd, struct client *clients,
                                    int nclients);
int is_subscribed_to_topic(char *topic, struct client *client);

#endif