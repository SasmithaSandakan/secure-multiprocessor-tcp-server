#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <errno.h>
#include <ctype.h>

#define PORT 50368
#define SID "1013"
#define BUFFER_SIZE 8192
#define MAX_PAYLOAD 4096

void send_response(int conn_fd, const char *status, int code, const char *message) {
    char response[512];
    snprintf(response, sizeof(response), "%s %d SID:%s %s\n", status, code, SID, message);
    send(conn_fd, response, strlen(response), 0);
}

int is_valid_number(const char *s) {
    if (s == NULL || *s == '\0') {
        return 0;
    }

    for (int i = 0; s[i] != '\0'; i++) {
        if (!isdigit((unsigned char)s[i])) {
            return 0;
        }
    }
    return 1;
}

int main() {
    int listen_fd, conn_fd;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len = sizeof(client_addr);

    char recv_buffer[BUFFER_SIZE];
    size_t buffered = 0;

    listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd < 0) {
        perror("socket failed");
        return 1;
    }

    int opt = 1;
    if (setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("setsockopt failed");
        close(listen_fd);
        return 1;
    }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    if (bind(listen_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("bind failed");
        close(listen_fd);
        return 1;
    }

    if (listen(listen_fd, 5) < 0) {
        perror("listen failed");
        close(listen_fd);
        return 1;
    }

    printf("A1 server listening on port %d\n", PORT);

    conn_fd = accept(listen_fd, (struct sockaddr *)&client_addr, &client_len);
    if (conn_fd < 0) {
        perror("accept failed");
        close(listen_fd);
        return 1;
    }

    printf("Client connected from %s:%d\n",
           inet_ntoa(client_addr.sin_addr),
           ntohs(client_addr.sin_port));

    while (1) {
        ssize_t bytes_received = recv(conn_fd,
                                      recv_buffer + buffered,
                                      sizeof(recv_buffer) - buffered - 1,
                                      0);

        if (bytes_received < 0) {
            perror("recv failed");
            break;
        }

        if (bytes_received == 0) {
            printf("Client disconnected.\n");
            break;
        }

        buffered += bytes_received;
        recv_buffer[buffered] = '\0';

        printf("\n[DEBUG] Received %zd bytes, buffered=%zu\n", bytes_received, buffered);

        while (1) {
            char *newline = memchr(recv_buffer, '\n', buffered);
            if (newline == NULL) {
                break;
            }

            size_t header_len = newline - recv_buffer;
            char header[128];

            if (header_len >= sizeof(header)) {
                send_response(conn_fd, "ERR", 400, "Header too long");
                buffered = 0;
                break;
            }

            memcpy(header, recv_buffer, header_len);
            header[header_len] = '\0';

            if (strncmp(header, "LEN:", 4) != 0) {
                send_response(conn_fd, "ERR", 400, "Invalid header format");
                size_t consume = header_len + 1;
                memmove(recv_buffer, recv_buffer + consume, buffered - consume);
                buffered -= consume;
                continue;
            }

            char *len_str = header + 4;

            if (!is_valid_number(len_str)) {
                send_response(conn_fd, "ERR", 400, "Invalid length");
                size_t consume = header_len + 1;
                memmove(recv_buffer, recv_buffer + consume, buffered - consume);
                buffered -= consume;
                continue;
            }

            long payload_len = strtol(len_str, NULL, 10);

            if (payload_len < 0) {
                send_response(conn_fd, "ERR", 400, "Negative length not allowed");
                size_t consume = header_len + 1;
                memmove(recv_buffer, recv_buffer + consume, buffered - consume);
                buffered -= consume;
                continue;
            }

            if (payload_len > MAX_PAYLOAD) {
                send_response(conn_fd, "ERR", 413, "Payload too large");
                size_t consume = header_len + 1;
                memmove(recv_buffer, recv_buffer + consume, buffered - consume);
                buffered -= consume;
                continue;
            }

            size_t total_needed = header_len + 1 + payload_len;

            if (buffered < total_needed) {
                printf("[DEBUG] Incomplete payload: need %zu total bytes, have %zu\n",
                       total_needed, buffered);
                break;
            }

            char payload[MAX_PAYLOAD + 1];
            memcpy(payload, recv_buffer + header_len + 1, payload_len);
            payload[payload_len] = '\0';

            printf("[DEBUG] Complete message parsed\n");
            printf("Header : %s\n", header);
            printf("Payload: %s\n", payload);

            send_response(conn_fd, "OK", 200, "Message received");

            memmove(recv_buffer, recv_buffer + total_needed, buffered - total_needed);
            buffered -= total_needed;
            recv_buffer[buffered] = '\0';
        }

        if (buffered == sizeof(recv_buffer) - 1) {
            send_response(conn_fd, "ERR", 413, "Internal buffer full");
            buffered = 0;
        }
    }

    close(conn_fd);
    close(listen_fd);
    return 0;
}
