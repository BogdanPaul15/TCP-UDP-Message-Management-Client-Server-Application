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

#include "include/client.h"
#include "include/common.h"
#include "include/protocols.h"
#include "include/utils.h"

int main(int argc, char *argv[]) {
    char buffer[BUFLEN], input[100], id[ID_LEN], ip[IP_LEN];
    struct pollfd pfds[2];
    struct sockaddr_in serv_addr;
    int rc, on = 1, nfds, tcp_sockfd;
    uint16_t port;

    // Disable buffering for stdout
    rc = setvbuf(stdout, NULL, _IONBF, BUFSIZ);
    DIE(rc != 0, "[SERV] Error disabling buffering for stdout");

    // Check the number of arguments
    DIE(argc != 4,
        "[CLI] Usage: ./subscriber <ID_CLIENT> <IP_SERVER> <PORT_SERVER>");

    // Parse the id
    DIE(strlen(argv[1]) > ID_LEN, "[CLI] ID_CLIENT is too long");
    memset(id, 0, ID_LEN);
    memcpy(id, argv[1], strlen(argv[1]));

    // Parse the ip
    DIE(strlen(argv[2]) > IP_LEN, "[CLI] Invalid IP format");
    memset(ip, 0, 16);
    memcpy(ip, argv[2], strlen(argv[2]));

    // Parse the port number
    port = (uint16_t)atoi(argv[3]);
    DIE(port == 0, "[CLI] Invalid port number");

    // Initialize the server address structure
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);
    rc = inet_aton(ip, &serv_addr.sin_addr);
    DIE(rc < 0, "[CLI] Error inet_atton");

    // Create the TCP socket
    tcp_sockfd = socket(AF_INET, SOCK_STREAM, 0);
    DIE(tcp_sockfd < 0, "[CLI] Error creating TCP socket");

    // Disable Nagle's algorithm for the TCP socket
    rc = setsockopt(tcp_sockfd, IPPROTO_TCP, TCP_NODELAY, (char *)&on,
                    sizeof(on));
    DIE(rc < 0, "[CLI] Error disabling Nagle's algorithm for TCP socket");

    // Connect to the server
    rc = connect(tcp_sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr));
    DIE(rc < 0, "[CLI] Error connecting to the server");

    // Initialize the poll structure
    memset(pfds, 0, sizeof(pfds));
    nfds = 0;

    pfds[nfds].fd = STDIN_FILENO;
    pfds[nfds].events = POLLIN;
    nfds++;

    pfds[nfds].fd = tcp_sockfd;
    pfds[nfds].events = POLLIN;
    nfds++;

    // Send the ID to the server
    rc = send(tcp_sockfd, id, ID_LEN, 0);
    DIE(rc < 0, "[CLI] Error sending id to server");

    while (1) {
        rc = poll(pfds, nfds, -1);
        DIE(rc < 0, "[CLI] Error on poll");
        memset(buffer, 0, BUFLEN);

        if (pfds[0].revents & POLLIN) {
            // Read a command from the standard input
            memset(input, 0, 100);
            fgets(input, 100 - 1, stdin);

            char *p = strtok(input, " ");
            if (p == NULL) {
                DIE(1, "[CLI] Invalid command");
            }
            char *command = p;

            // Check if the command is 'exit'
            if (strncmp(command, "exit", 4) == 0) {
                // Send the exit command to the server
                uint8_t command = EXIT;
                uint32_t length = sizeof(command);
                memcpy(buffer, &length, sizeof(uint32_t));
                memcpy(buffer + sizeof(uint32_t), &command, sizeof(uint8_t));

                rc = send_all(tcp_sockfd, buffer,
                              sizeof(uint32_t) + sizeof(uint8_t));
                DIE(rc < 0, "[CLI] Error sending exit command to server");

                break;
            } else {
                p = strtok(NULL, " ");
                if (p == NULL) {
                    DIE(1, "[CLI] Invalid command");
                }
                char *topic = p;
                topic[strlen(topic) - 1] = '\0';

                // Check if command is 'subscribe'
                if (strcmp(command, "subscribe") == 0) {
                    uint8_t command = SUBSCRIBE;
                    uint32_t length = strlen(topic) + 1;
                    memcpy(buffer, &length, sizeof(uint32_t));
                    memcpy(buffer + sizeof(uint32_t), &command,
                           sizeof(uint8_t));
                    memcpy(buffer + sizeof(uint32_t) + sizeof(uint8_t), topic,
                           length);

                    // Send the subscribe command to the server
                    rc =
                        send_all(tcp_sockfd, buffer, sizeof(uint32_t) + length);
                    DIE(rc < 0,
                        "[CLI] Error sending subscribe command to server");

                    printf("Subscribed to topic %s\n", topic);
                    continue;
                }

                // Check if command is 'unsubscribe'
                if (strcmp(command, "unsubscribe") == 0) {
                    uint8_t command = UNSUBSCRIBE;
                    uint32_t length = strlen(topic) + 1;
                    memcpy(buffer, &length, sizeof(uint32_t));
                    memcpy(buffer + sizeof(uint32_t), &command,
                           sizeof(uint8_t));
                    memcpy(buffer + sizeof(uint32_t) + sizeof(uint8_t), topic,
                           length);

                    // Send the unsubscribe command to the server
                    rc =
                        send_all(tcp_sockfd, buffer, sizeof(uint32_t) + length);
                    DIE(rc < 0,
                        "[CLI] Error sending unsubscribe command to server");

                    printf("Unsubscribed from topic %s\n", topic);
                    continue;
                }
                fprintf(stderr, "[CLI] Unknown command\n");
                continue;
            }
        }

        if (pfds[1].revents & POLLIN) {
            // Receive a message from the server
            rc = recv_all(tcp_sockfd, &buffer, BUFLEN);
            DIE(rc < 0, "[CLI] Error receiving message from server");

            if (rc == 0) {
                rc = close(tcp_sockfd);
                DIE(rc < 0, "[CLI] Error closing TCP socket");
                return 0;
            }

            // Print the message into client
            printf("%s", buffer);
        }
    }

    // Close the client socket
    rc = close(tcp_sockfd);
    DIE(rc < 0, "[CLI] Error closing TCP socket");
    return 0;
}