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
#include <time.h>
#include <openssl/sha.h>

#define PORT 50368
#define SID "1013"
#define BUFFER_SIZE 8192
#define MAX_PAYLOAD 4096
#define USERS_FILE "users_1368.txt"
#define TOKEN_SIZE 64
#define SESSION_TIMEOUT 300

/* A4 protection settings */
#define RATE_LIMIT_MAX_REQUESTS 5
#define RATE_LIMIT_WINDOW 10
#define LOGIN_FAIL_LIMIT 3
#define LOGIN_LOCKOUT_SECONDS 30

typedef struct {
    int logged_in;
    char username[64];
    char token[TOKEN_SIZE];
    time_t last_activity;

    /* A4: rate limiting fields */
    int request_count;
    time_t window_start;

    /* A4: brute-force protection fields */
    int failed_login_attempts;
    time_t lockout_until;
} Session;

/* ---------------- RESPONSE HELPERS ---------------- */

void send_response(int conn_fd, const char *status, int code, const char *message) {
    char response[512];
    snprintf(response, sizeof(response), "%s %d SID:%s %s\n", status, code, SID, message);
    send(conn_fd, response, strlen(response), 0);
}

void send_response_with_token(int conn_fd, const char *status, int code, const char *message, const char *token) {
    char response[512];
    snprintf(response, sizeof(response), "%s %d SID:%s %s TOKEN:%s\n", status, code, SID, message, token);
    send(conn_fd, response, strlen(response), 0);
}

/* ---------------- BASIC VALIDATION ---------------- */

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

int is_valid_username(const char *username) {
    size_t len = strlen(username);

    if (len < 3 || len > 32) {
        return 0;
    }

    for (size_t i = 0; i < len; i++) {
        if (!(isalnum((unsigned char)username[i]) || username[i] == '_')) {
            return 0;
        }
    }

    return 1;
}

/* ---------------- HASH / SALT / TOKEN ---------------- */

void generate_random_hex(char *output, size_t hex_length) {
    const char *hex = "0123456789abcdef";

    for (size_t i = 0; i < hex_length; i++) {
        output[i] = hex[rand() % 16];
    }

    output[hex_length] = '\0';
}

void sha256_hex(const char *input, char *output_hex) {
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256((const unsigned char *)input, strlen(input), hash);

    for (int i = 0; i < SHA256_DIGEST_LENGTH; i++) {
        sprintf(output_hex + (i * 2), "%02x", hash[i]);
    }

    output_hex[64] = '\0';
}

void hash_password_with_salt(const char *password, const char *salt, char *output_hash) {
    char combined[256];
    snprintf(combined, sizeof(combined), "%s%s", salt, password);
    sha256_hex(combined, output_hash);
}

void generate_token(char *token_out) {
    generate_random_hex(token_out, 32);
}

/* ---------------- USER STORAGE ---------------- */

int user_exists(const char *username) {
    FILE *fp = fopen(USERS_FILE, "r");
    if (fp == NULL) {
        return 0;
    }

    char line[512];
    char file_user[64], salt[64], hash[128];

    while (fgets(line, sizeof(line), fp)) {
        if (sscanf(line, "%63[^:]:%63[^:]:%127s", file_user, salt, hash) == 3) {
            if (strcmp(file_user, username) == 0) {
                fclose(fp);
                return 1;
            }
        }
    }

    fclose(fp);
    return 0;
}

int register_user(const char *username, const char *password) {
    if (user_exists(username)) {
        return 0;
    }

    FILE *fp = fopen(USERS_FILE, "a");
    if (fp == NULL) {
        return -1;
    }

    char salt[33];
    char hash[65];

    generate_random_hex(salt, 32);
    hash_password_with_salt(password, salt, hash);

    fprintf(fp, "%s:%s:%s\n", username, salt, hash);
    fclose(fp);

    return 1;
}

int verify_user_login(const char *username, const char *password) {
    FILE *fp = fopen(USERS_FILE, "r");
    if (fp == NULL) {
        return 0;
    }

    char line[512];
    char file_user[64], salt[64], stored_hash[128];
    char computed_hash[65];

    while (fgets(line, sizeof(line), fp)) {
        if (sscanf(line, "%63[^:]:%63[^:]:%127s", file_user, salt, stored_hash) == 3) {
            if (strcmp(file_user, username) == 0) {
                hash_password_with_salt(password, salt, computed_hash);
                fclose(fp);

                if (strcmp(computed_hash, stored_hash) == 0) {
                    return 1;
                } else {
                    return 0;
                }
            }
        }
    }

    fclose(fp);
    return 0;
}

/* ---------------- SESSION HELPERS ---------------- */

void session_login(Session *session, const char *username) {
    session->logged_in = 1;
    strncpy(session->username, username, sizeof(session->username) - 1);
    session->username[sizeof(session->username) - 1] = '\0';
    generate_token(session->token);
    session->last_activity = time(NULL);

    /* successful login resets login failure state */
    session->failed_login_attempts = 0;
    session->lockout_until = 0;
}

void session_logout(Session *session) {
    session->logged_in = 0;
    session->username[0] = '\0';
    session->token[0] = '\0';
    session->last_activity = 0;
}

int session_is_expired(Session *session) {
    if (!session->logged_in) {
        return 1;
    }

    time_t now = time(NULL);
    double diff = difftime(now, session->last_activity);

    if (diff > SESSION_TIMEOUT) {
        return 1;
    }

    return 0;
}

int session_is_valid(Session *session) {
    if (!session->logged_in) {
        return 0;
    }

    if (session_is_expired(session)) {
        session_logout(session);
        return 0;
    }

    session->last_activity = time(NULL);
    return 1;
}

/* ---------------- A4 PROTECTION HELPERS ---------------- */

int check_rate_limit(Session *session) {
    time_t now = time(NULL);

    if (session->window_start == 0) {
        session->window_start = now;
        session->request_count = 1;
        return 1;
    }

    if (difftime(now, session->window_start) > RATE_LIMIT_WINDOW) {
        session->window_start = now;
        session->request_count = 1;
        return 1;
    }

    session->request_count++;

    if (session->request_count > RATE_LIMIT_MAX_REQUESTS) {
        return 0;
    }

    return 1;
}

int login_is_locked(Session *session) {
    time_t now = time(NULL);

    if (session->lockout_until == 0) {
        return 0;
    }

    if (now >= session->lockout_until) {
        session->lockout_until = 0;
        session->failed_login_attempts = 0;
        return 0;
    }

    return 1;
}

void record_failed_login(Session *session) {
    session->failed_login_attempts++;

    if (session->failed_login_attempts >= LOGIN_FAIL_LIMIT) {
        session->lockout_until = time(NULL) + LOGIN_LOCKOUT_SECONDS;
    }
}

/* ---------------- SIGNAL HANDLER ---------------- */

void handle_sigchld(int sig) {
    (void)sig;
    int saved_errno = errno;

    while (waitpid(-1, NULL, WNOHANG) > 0) {
        /* reap children */
    }

    errno = saved_errno;
}

/* ---------------- COMMAND HANDLER ---------------- */

void process_command(int conn_fd, char *payload, Session *session) {
    char command[32];
    char arg1[64];
    char arg2[64];

    memset(command, 0, sizeof(command));
    memset(arg1, 0, sizeof(arg1));
    memset(arg2, 0, sizeof(arg2));

    if (!check_rate_limit(session)) {
        send_response(conn_fd, "ERR", 429, "Rate limit exceeded");
        return;
    }

    int count = sscanf(payload, "%31s %63s %63s", command, arg1, arg2);

    if (count <= 0) {
        send_response(conn_fd, "ERR", 400, "Empty command");
        return;
    }

    if (strcmp(command, "REGISTER") == 0) {
        if (count != 3) {
            send_response(conn_fd, "ERR", 400, "Usage: REGISTER <user> <pass>");
            return;
        }

        if (!is_valid_username(arg1)) {
            send_response(conn_fd, "ERR", 400, "Invalid username");
            return;
        }

        if (strlen(arg2) < 4) {
            send_response(conn_fd, "ERR", 400, "Password too short");
            return;
        }

        int result = register_user(arg1, arg2);

        if (result == 1) {
            send_response(conn_fd, "OK", 201, "User registered");
        } else if (result == 0) {
            send_response(conn_fd, "ERR", 409, "User already exists");
        } else {
            send_response(conn_fd, "ERR", 500, "Could not store user");
        }

        return;
    }

    if (strcmp(command, "LOGIN") == 0) {
        if (count != 3) {
            send_response(conn_fd, "ERR", 400, "Usage: LOGIN <user> <pass>");
            return;
        }

        if (login_is_locked(session)) {
            send_response(conn_fd, "ERR", 423, "Login temporarily locked");
            return;
        }

        if (verify_user_login(arg1, arg2)) {
            session_login(session, arg1);
            send_response_with_token(conn_fd, "OK", 200, "Login successful", session->token);
        } else {
            record_failed_login(session);
            if (login_is_locked(session)) {
                send_response(conn_fd, "ERR", 423, "Too many failed logins, temporarily locked");
            } else {
                send_response(conn_fd, "ERR", 401, "Invalid username or password");
            }
        }

        return;
    }

    if (strcmp(command, "WHOAMI") == 0) {
        if (!session_is_valid(session)) {
            send_response(conn_fd, "ERR", 401, "Authentication required or session expired");
            return;
        }

        char msg[128];
        snprintf(msg, sizeof(msg), "You are logged in as %s", session->username);
        send_response(conn_fd, "OK", 200, msg);
        return;
    }

    if (strcmp(command, "LOGOUT") == 0) {
        if (!session->logged_in) {
            send_response(conn_fd, "ERR", 400, "Not logged in");
            return;
        }

        session_logout(session);
        send_response(conn_fd, "OK", 200, "Logged out");
        return;
    }

    send_response(conn_fd, "ERR", 400, "Unknown command");
}

/* ---------------- CLIENT SESSION ---------------- */

void handle_client_session(int conn_fd, struct sockaddr_in client_addr) {
    char recv_buffer[BUFFER_SIZE];
    size_t buffered = 0;
    Session session;

    memset(&session, 0, sizeof(session));

    printf("\n========================================\n");
    printf("Child PID %d handling client %s:%d\n",
           getpid(),
           inet_ntoa(client_addr.sin_addr),
           ntohs(client_addr.sin_port));
    printf("========================================\n");

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
            printf("Child PID %d: client disconnected.\n", getpid());
            break;
        }

        buffered += bytes_received;
        recv_buffer[buffered] = '\0';

        printf("\n[Child %d DEBUG] Received %zd bytes, buffered=%zu\n",
               getpid(), bytes_received, buffered);

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
                printf("[Child %d DEBUG] Incomplete payload: need %zu bytes, have %zu\n",
                       getpid(), total_needed, buffered);
                break;
            }

            char payload[MAX_PAYLOAD + 1];
            memcpy(payload, recv_buffer + header_len + 1, payload_len);
            payload[payload_len] = '\0';

            printf("[Child %d DEBUG] Complete message parsed\n", getpid());
            printf("[Child %d DEBUG] Header : %s\n", getpid(), header);
            printf("[Child %d DEBUG] Payload: %s\n", getpid(), payload);

            process_command(conn_fd, payload, &session);

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

    printf("Child PID %d finished.\n", getpid());
    printf("========================================\n\n");
}

/* ---------------- MAIN ---------------- */

int main() {
    int listen_fd, conn_fd;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len;
    struct sigaction sa;

    setbuf(stdout, NULL);
    srand((unsigned int)(time(NULL) ^ getpid()));

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

    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handle_sigchld;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;

    if (sigaction(SIGCHLD, &sa, NULL) < 0) {
        perror("sigaction failed");
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

    if (listen(listen_fd, 10) < 0) {
        perror("listen failed");
        close(listen_fd);
        return 1;
    }

    printf("A4 server listening on port %d (Parent PID: %d)\n", PORT, getpid());

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
            close(listen_fd);
            handle_client_session(conn_fd, client_addr);
            exit(0);
        } else {
            printf("Parent PID %d created child PID %d\n\n", getpid(), pid);
            close(conn_fd);
        }
    }

    close(listen_fd);
    return 0;
}
