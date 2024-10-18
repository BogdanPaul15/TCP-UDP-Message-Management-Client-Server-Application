#include <math.h>
#include <netinet/in.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "protocols.h"
#include "utils.h"

#define BUFLEN 1600

int send_all(int sockfd, void *buffer, size_t len);
int recv_all(int sockfd, void *buffer, size_t len);
void format_message(struct udppkt *received_pkt, struct sockaddr_in udp_addr,
                    char *message);