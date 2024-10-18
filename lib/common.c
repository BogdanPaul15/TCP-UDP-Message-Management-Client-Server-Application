#include "common.h"

#include <arpa/inet.h>
#include <sys/socket.h>

int send_all(int sockfd, void *buffer, size_t len) {
    size_t bytes_sent = 0;
    size_t bytes_remaining = len;
    char *buff = buffer;

    // Send len bytes
    while (bytes_remaining) {
        int bytes = send(sockfd, buff + bytes_sent, bytes_remaining, 0);
        if (bytes < 0) {
            return -1;
        }
        bytes_sent += bytes;
        bytes_remaining -= bytes;
    }

    return bytes_sent;
}

int recv_all(int sockfd, void *buffer, size_t len) {
    uint32_t msg_len;
    size_t bytes_received = 0;
    size_t bytes_remaining = sizeof(msg_len);
    char *buff = (char *)&msg_len;

    // First get the 4 bytes which are the length of the packet
    while (bytes_remaining > 0) {
        int bytes = recv(sockfd, buff + bytes_received, bytes_remaining, 0);
        if (bytes <= 0) {
            return -1;
        }
        bytes_received += bytes;
        bytes_remaining -= bytes;
    }

    bytes_received = 0;
    bytes_remaining = msg_len;
    buff = buffer;

    // Get length bytes of the packet
    while (bytes_remaining > 0) {
        int bytes = recv(sockfd, buff + bytes_received, bytes_remaining, 0);
        if (bytes <= 0) {
            return -1;
        }
        bytes_received += bytes;
        bytes_remaining -= bytes;
    }

    return bytes_received;
}

void format_message(struct udppkt *received_pkt, struct sockaddr_in udp_addr,
                    char *message) {
    double value = 0;
    switch (received_pkt->type) {
        case 0:
            // First byte is the sign, next 4 bytes are the value
            uint32_t value_int =
                ntohl(*((uint32_t *)(received_pkt->content + 1)));

            sprintf(message, "%s:%d - %s - INT - %d\n",
                    inet_ntoa(udp_addr.sin_addr), ntohs(udp_addr.sin_port),
                    received_pkt->topic,
                    received_pkt->content[0] == 1 ? -value_int : value_int);
            break;
        case 1:
            // The value is a short real, so it is a 2 byte integer
            value = ntohs(*(uint16_t *)(received_pkt->content));
            value /= 100;

            sprintf(message, "%s:%d - %s - SHORT_REAL - %.2f\n",
                    inet_ntoa(udp_addr.sin_addr), ntohs(udp_addr.sin_port),
                    received_pkt->topic, value);
            break;
        case 2:
            // First byte is the sign, next 4 bytes are the value
            value = ntohl(*(uint32_t *)(received_pkt->content + 1));

            // The power is the last byte
            uint8_t power = (uint8_t)received_pkt->content[5];
            value /= pow(10, power);

            sprintf(message, "%s:%d - %s - FLOAT - %.8g\n",
                    inet_ntoa(udp_addr.sin_addr), ntohs(udp_addr.sin_port),
                    received_pkt->topic,
                    received_pkt->content[0] == 1 ? -value : value);
            break;
        case 3:
            // The content is a string
            received_pkt->content[strlen(received_pkt->content)] = '\0';
            sprintf(message, "%s:%d - %s - STRING - %s\n",
                    inet_ntoa(udp_addr.sin_addr), ntohs(udp_addr.sin_port),
                    received_pkt->topic, received_pkt->content);
            break;
        default:
            DIE(1, "[SERV] Unknown type");
    }
}
