#include "client.h"

#include <string.h>

struct client *get_client_by_id(char *id, struct client *clients,
                                int nclients) {
    for (int i = 0; i < nclients; i++) {
        if (strncmp(clients[i].id, id, strlen(id)) == 0) {
            return &clients[i];
        }
    }
    return NULL;
}

struct client *get_client_by_sockfd(int sockfd, struct client *clients,
                                    int nclients) {
    for (int i = 0; i < nclients; i++) {
        if (clients[i].sockfd == sockfd) {
            return &clients[i];
        }
    }
    return NULL;
}

int is_subscribed_to_topic(char *topic, struct client *client) {
    for (int i = 0; i < client->ntopics; i++) {
        if (strcmp(client->topics[i].name, topic) == 0) {
            return 1;
        }
    }
    return 0;
}