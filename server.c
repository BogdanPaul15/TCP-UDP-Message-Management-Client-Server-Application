#include <arpa/inet.h>
#include <math.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <poll.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "client.h"
#include "common.h"
#include "protocols.h"
#include "utils.h"

int main(int argc, char *argv[]) {
    char buffer[BUFLEN], topic[51];
    struct pollfd pfds[MAX_CONNECTIONS];
    struct client *clients;
    int rc, on = 1, nfds, udp_sockfd, tcp_sockfd, newsockfd, nclients = 0;
    struct sockaddr_in serv_addr, cli_addr;
    uint16_t port;

    // Disable buffering for stdout
    rc = setvbuf(stdout, NULL, _IONBF, BUFSIZ);
    DIE(rc != 0, "[SERV] Error disabling buffering for stdout");

    // Check the number of arguments
    DIE(argc != 2, "[SERV] Usage: ./server <PORT_SERVER>");

    // Parse the port number
    port = (uint16_t)atoi(argv[1]);
    DIE(port == 0, "[SERV] Invalid port number");

    // Create the TCP socket
    tcp_sockfd = socket(AF_INET, SOCK_STREAM, 0);
    DIE(tcp_sockfd < 0, "[SERV] Error creating TCP socket");

    // Create the UDP socket
    udp_sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    DIE(udp_sockfd < 0, "[SERV] Error creating UDP socket");

    // Disable Nagle's algorithm for the TCP socket
    rc = setsockopt(tcp_sockfd, IPPROTO_TCP, TCP_NODELAY, (char *)&on,
                    sizeof(on));
    DIE(rc < 0, "[SERV] Error disabling Nagle's algorithm for TCP socket");

    // Initialize the server address structure
    memset((char *)&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);
    serv_addr.sin_addr.s_addr = INADDR_ANY;

    // Bind the TCP socket
    rc = bind(tcp_sockfd, (struct sockaddr *)&serv_addr,
              sizeof(struct sockaddr));
    DIE(rc < 0, "[SERV] Error binding TCP socket");

    // Bind the UDP socket
    rc = bind(udp_sockfd, (struct sockaddr *)&serv_addr,
              sizeof(struct sockaddr));
    DIE(rc < 0, "[SERV] Error binding UDP socket");

    // Listen for incoming connections
    rc = listen(tcp_sockfd, MAX_CONNECTIONS);
    DIE(rc < 0, "[SERV] Error listening for TCP connections");

    // Initialize the poll structure
    memset(pfds, 0, sizeof(pfds));
    nfds = 0;

    pfds[nfds].fd = STDIN_FILENO;
    pfds[nfds].events = POLLIN;
    nfds++;

    pfds[nfds].fd = tcp_sockfd;
    pfds[nfds].events = POLLIN;
    nfds++;

    pfds[nfds].fd = udp_sockfd;
    pfds[nfds].events = POLLIN;
    nfds++;

    while (1) {
        rc = poll(pfds, nfds, -1);
        DIE(rc < 0, "[SERV] Error on poll");
        memset(buffer, 0, BUFLEN);

        if (pfds[0].revents & POLLIN) {
            // Read a command from the standard input
            char buf[BUFLEN];
            memset(buf, 0, BUFLEN);
            fgets(buf, BUFLEN - 1, stdin);

            // Check if the command is 'exit'
            if (strncmp(buf, "exit", 4) == 0) {
                // Disconnect all clients
                for (int i = 0; i < nclients; i++) {
                    if (clients[i].state == CONNECTED) {
                        // Close the connection with the client
                        rc = close(clients[i].sockfd);
                        DIE(rc < 0, "[SERV] Error closing client socket");
                    }
                }
                break;
            }
            fprintf(stderr, "[SERV] Unknown command\n");
            continue;
        }

        if (pfds[1].revents & POLLIN) {
            // Accept a new TCP connection
            socklen_t clilen = sizeof(cli_addr);
            newsockfd =
                accept(tcp_sockfd, (struct sockaddr *)&cli_addr, &clilen);
            DIE(newsockfd < 0, "[SERV] Error accepting TCP connection");

            // Disable Nagle's algorithm for the new TCP connection
            rc = setsockopt(newsockfd, SOL_SOCKET, SO_REUSEADDR, (char *)&on,
                            sizeof(on));
            DIE(rc < 0, "[SERV] Error disabling Nagle's algorithm for TCP");

            char id[ID_LEN];
            memset(id, 0, ID_LEN);
            rc = recv(newsockfd, id, ID_LEN, 0);
            DIE(rc < 0, "[SERV] Error receiving id from client");

            // Verify if the client is already connected
            struct client *cl = get_client_by_id(id, clients, nclients);
            if (cl == NULL) {
                // Add the new client to the poll structure
                pfds[nfds].fd = newsockfd;
                pfds[nfds].events = POLLIN;
                nfds++;

                if (clients == NULL) {
                    // First client
                    clients = (struct client *)malloc(sizeof(struct client));
                    DIE(clients == NULL, "[SERV] Error allocating memory");
                } else {
                    // More clients
                    clients = (struct client *)realloc(
                        clients, (nclients + 1) * sizeof(struct client));
                    DIE(clients == NULL, "[SERV] Error reallocating memory");
                }

                // Add the new client to the list
                memset(clients[nclients].id, 0, ID_LEN);
                memcpy(clients[nclients].id, id, ID_LEN);
                clients[nclients].topics = NULL;
                clients[nclients].ntopics = 0;
                clients[nclients].sockfd = newsockfd;
                clients[nclients].state = CONNECTED;
                nclients++;

                printf("New client %s connected from %s:%d.\n", id,
                       inet_ntoa(cli_addr.sin_addr), ntohs(cli_addr.sin_port));
            } else if (cl->state == CONNECTED) {
                // Client is already connected
                printf("Client %s already connected.\n", id);

                // Close the new TCP connection
                rc = close(newsockfd);
                DIE(rc < 0, "[SERV] Error closing client socket");
            } else {
                // Client wants to reconnect
                cl->sockfd = newsockfd;
                cl->state = CONNECTED;
                printf("New client %s connected from %s:%d.\n", id,
                       inet_ntoa(cli_addr.sin_addr), ntohs(cli_addr.sin_port));
            }
            continue;
        }

        if (pfds[2].revents & POLLIN) {
            // UDP Client
            struct udppkt *received_pkt;
            struct sockaddr_in udp_addr;
            socklen_t udp_addr_len = sizeof(udp_addr);
            char message[BUFLEN];

            // Receive a udp packet from the client
            rc = recvfrom(udp_sockfd, buffer, BUFLEN, 0,
                          (struct sockaddr *)&udp_addr, &udp_addr_len);
            DIE(rc < 0, "[SERV] Error receiving UDP packet");

            received_pkt = (struct udppkt *)buffer;
            memset(message, 0, sizeof(message));

            // Format the message
            format_message(received_pkt, udp_addr, message);

            // Add the length of the message at the beginning
            uint32_t length = strlen(message) + 1;
            memmove(message + sizeof(uint32_t), message, length);
            memcpy(message, &length, sizeof(uint32_t));

            // Send the message to all TCP clients
            for (int i = 0; i < nclients; i++) {
                // Verify if client is subscribed to the topic
                if (is_subscribed_to_topic(received_pkt->topic, &clients[i])) {
                    if (clients[i].state == CONNECTED) {
                        // Send the message to the client using send_all
                        rc = send_all(clients[i].sockfd, &message,
                                      length + sizeof(uint32_t));
                        DIE(rc < 0,
                            "[SERV] Error sending message to TCP client");
                    }
                }
            }
            continue;
        }

        for (int i = 3; i < nfds; i++) {
            if (pfds[i].revents & POLLIN) {
                // Receive a message from a TCP client
                struct client *cl =
                    get_client_by_sockfd(pfds[i].fd, clients, nclients);
                memset(buffer, 0, BUFLEN);
                rc = recv_all(pfds[i].fd, &buffer, sizeof(buffer));
                DIE(rc < 0, "[SERV] Error receiving message from TCP client");

                // First byte of payload is the command
                uint8_t command = *(uint8_t *)buffer;

                // After the command comes the topic
                memset(topic, 0, sizeof(topic));
                memcpy(topic, buffer + sizeof(uint8_t),
                       strlen(buffer + sizeof(uint8_t)));

                // Check the command
                switch (command) {
                    case SUBSCRIBE:
                        // Add the topic to the client
                        if (is_subscribed_to_topic(topic, cl) == 0) {
                            if (cl->topics == NULL) {
                                // First topic
                                cl->topics = (struct topic *)malloc(
                                    sizeof(struct topic));
                                DIE(cl->topics == NULL,
                                    "[SERV] Error allocating memory");
                            } else {
                                // More topics
                                cl->topics = (struct topic *)realloc(
                                    cl->topics,
                                    (cl->ntopics + 1) * sizeof(struct topic));
                                DIE(cl->topics == NULL,
                                    "[SERV] Error reallocating memory");
                            }
                            memcpy(cl->topics[cl->ntopics].name, topic,
                                   strlen(topic) + 1);
                            cl->ntopics++;
                        }
                        break;
                    case UNSUBSCRIBE:
                        // Remove the topic from the client
                        for (int j = 0; j < cl->ntopics; j++) {
                            if (strcmp(cl->topics[j].name, topic) == 0) {
                                for (int k = j; k < cl->ntopics - 1; k++) {
                                    memset(cl->topics[k].name, 0,
                                           sizeof(cl->topics[k].name));
                                    memcpy(cl->topics[k].name,
                                           cl->topics[k + 1].name,
                                           strlen(cl->topics[k + 1].name) + 1);
                                }
                                cl->topics = (struct topic *)realloc(
                                    cl->topics,
                                    (cl->ntopics - 1) * sizeof(struct topic));
                                cl->ntopics--;
                                break;
                            }
                        }
                        break;
                    case EXIT:
                        // Close the connection with the client
                        cl->state = DISCONNECTED;
                        rc = close(pfds[i].fd);
                        DIE(rc < 0, "[SERV] Error closing client socket");
                        printf("Client %s disconnected.\n", cl->id);
                        break;
                    default:
                        DIE(1, "[SERV] Unknown command");
                }
            }
        }
        continue;
    }

    // Close the sockets
    rc = close(tcp_sockfd);
    DIE(rc < 0, "[SERV] Error closing TCP socket");
    rc = close(udp_sockfd);
    DIE(rc < 0, "[SERV] Error closing UDP socket");

    // Free the memory
    free(clients);
    return 0;
}