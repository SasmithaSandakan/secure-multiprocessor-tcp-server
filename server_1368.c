#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <errno.h>
#include <ctype.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/types.h>

#define PORT 50368
#define SID "1013"
#define BUFFER_SIZE 8192
#define MAX_PAYLOAD 4096

/*
   Send a formatted reply to the client.

   Example success:
   OK 200 SID:1013 Message received

   Example error:
   ERR 400 SID:1013 Invalid length
*/
void send_response(int conn_fd, const char *status, int code, const char *message) {
    char response[512];
    snprintf(response, sizeof(response), "%s %d SID:%s %s\n", status, code, SID, message);
    send(conn_fd, response, strlen(response), 0);
}

/*
   Check whether a string contains only digits.
   This is used to validate the number after LEN:
*/
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

/*
   SIGCHLD handler:
   When child processes finish, the parent receives SIGCHLD.
   We use waitpid(..., WNOHANG) to clean them immediately.
   This prevents zombie processes.
*/
void handle_sigchld(int sig) {
    (void)sig;  // unused parameter

    int saved_errno = errno;  // preserve errno

    while (waitpid(-1, NULL, WNOHANG) > 0) {
        /* Reap all finished child processes */
    }

    errno = saved_errno;  // restore errno
}

/*
   Handle one client session.

   This function runs inside the child process.
   It receives data from one client, parses framed messages,
   and sends responses back to that client.
*/
void handle_client_session(int conn_fd, struct sockaddr_in client_addr) {
    char recv_buffer[BUFFER_SIZE];
    size_t buffered = 0;

    printf("\n========================================\n");
    printf("Child PID %d handling client %s:%d\n",
           getpid(),
           inet_ntoa(client_addr.sin_addr),
           ntohs(client_addr.sin_port));
    printf("========================================\n");

    while (1) {
        /*
           Read new bytes into the unused part of the buffer.
           We append incoming TCP data to what is already buffered.
        */
        ssize_t bytes_received = recv(conn_fd,
                                      recv_buffer + buffered,
                                      sizeof(recv_buffer) - buffered - 1,
                                      0);

        if (bytes_received < 0) {
            perror("recv failed");
            break;
        }

        if (bytes_received == 0) {
            printf("Child PID %d: client disconnected.\n", getpid());
            break;
        }

        buffered += bytes_received;
        recv_buffer[buffered] = '\0';

        printf("\n[Child %d DEBUG] Received %zd bytes, buffered=%zu\n",
               getpid(), bytes_received, buffered);

        /*
           Parse as many complete framed messages as possible
           from the current buffer.
        */
        while (1) {
            char *newline = memchr(recv_buffer, '\n', buffered);

            /* No full header yet */
            if (newline == NULL) {
                break;
            }

            size_t header_len = newline - recv_buffer;
            char header[128];

            /* Protect against abnormally long headers */
            if (header_len >= sizeof(header)) {
                send_response(conn_fd, "ERR", 400, "Header too long");
                buffered = 0;
                break;
            }

            memcpy(header, recv_buffer, header_len);
            header[header_len] = '\0';

            /* Header must begin with LEN: */
            if (strncmp(header, "LEN:", 4) != 0) {
                send_response(conn_fd, "ERR", 400, "Invalid header format");

                size_t consume = header_len + 1;
                memmove(recv_buffer, recv_buffer + consume, buffered - consume);
                buffered -= consume;
                continue;
            }

            char *len_str = header + 4;

            /* Length must be numeric */
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

            /* Reject payloads larger than assignment limit */
            if (payload_len > MAX_PAYLOAD) {
                send_response(conn_fd, "ERR", 413, "Payload too large");

                size_t consume = header_len + 1;
                memmove(recv_buffer, recv_buffer + consume, buffered - consume);
                buffered -= consume;
                continue;
            }

            /*
               Total bytes needed for one complete frame:
               header bytes + newline + payload bytes
            */
            size_t total_needed = header_len + 1 + payload_len;

            /* Wait for more bytes if payload is incomplete */
            if (buffered < total_needed) {
                printf("[Child %d DEBUG] Incomplete payload: need %zu bytes, have %zu\n",
                       getpid(), total_needed, buffered);
                break;
            }

            /* Extract payload */
            char payload[MAX_PAYLOAD + 1];
            memcpy(payload, recv_buffer + header_len + 1, payload_len);
            payload[payload_len] = '\0';

            printf("[Child %d DEBUG] Complete message parsed\n", getpid());
            printf("[Child %d DEBUG] Header : %s\n", getpid(), header);
            printf("[Child %d DEBUG] Payload: %s\n", getpid(), payload);

            /*
               For A1/A2 we only acknowledge the message.
               Actual command logic (REGISTER, LOGIN, etc.) will come later.
            */
            send_response(conn_fd, "OK", 200, "Message received");

            /*
               Remove the processed frame from the buffer.
               If another frame is already present, the loop continues
               and parses it too.
            */
            memmove(recv_buffer, recv_buffer + total_needed, buffered - total_needed);
            buffered -= total_needed;
            recv_buffer[buffered] = '\0';
        }

        /* Safety check if internal receive buffer becomes full */
        if (buffered == sizeof(recv_buffer) - 1) {
            send_response(conn_fd, "ERR", 413, "Internal buffer full");
            buffered = 0;
        }
    }

    close(conn_fd);

    printf("Child PID %d finished.\n", getpid());
    printf("========================================\n\n");
}

/*
   Main server process:
   - creates listening socket
   - installs SIGCHLD handler
   - accepts clients forever
   - forks a new child for each client
*/
int main() {
    int listen_fd, conn_fd;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len;
    struct sigaction sa;

    /* Make stdout unbuffered so parent/child output appears immediately */
    setbuf(stdout, NULL);

    /* Create TCP socket */
    listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd < 0) {
        perror("socket failed");
        return 1;
    }

    /* Allow quick server restart */
    int opt = 1;
    if (setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("setsockopt failed");
        close(listen_fd);
        return 1;
    }

    /* Install SIGCHLD handler to clean finished children */
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handle_sigchld;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;

    if (sigaction(SIGCHLD, &sa, NULL) < 0) {
        perror("sigaction failed");
        close(listen_fd);
        return 1;
    }

    /* Prepare server address */
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    /* Bind socket to required port */
    if (bind(listen_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("bind failed");
        close(listen_fd);
        return 1;
    }

    /* Start listening for incoming TCP connections */
    if (listen(listen_fd, 10) < 0) {
        perror("listen failed");
        close(listen_fd);
        return 1;
    }

    printf("A2 server listening on port %d (Parent PID: %d)\n", PORT, getpid());

    /* Parent process accepts clients forever */
    while (1) {
        client_len = sizeof(client_addr);

        conn_fd = accept(listen_fd, (struct sockaddr *)&client_addr, &client_len);
        if (conn_fd < 0) {
            perror("accept failed");
            continue;
        }

        printf("\n----------------------------------------\n");
        printf("Parent PID %d accepted connection from %s:%d\n",
               getpid(),
               inet_ntoa(client_addr.sin_addr),
               ntohs(client_addr.sin_port));
        printf("----------------------------------------\n");

        pid_t pid = fork();

        if (pid < 0) {
            perror("fork failed");
            close(conn_fd);
            continue;
        }

        if (pid == 0) {
            /* Child process */

            close(listen_fd);  // child should not accept new clients
            handle_client_session(conn_fd, client_addr);
            exit(0);
        } else {
            /* Parent process */

            printf("Parent PID %d created child PID %d\n\n", getpid(), pid);
            close(conn_fd);  // parent should not handle this client session directly
        }
    }

    close(listen_fd);
    return 0;
}
