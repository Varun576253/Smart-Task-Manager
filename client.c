/*
 * client.c
 * Smart Task Manager – Terminal Client
 *
 * Connects to server via TCP, handles login, then enters an
 * interactive command loop.  All input/output is line-based text.
 *
 * Compile:  see Makefile
 * Usage:    ./client [host] [port]
 *           Default host: 127.0.0.1   Default port: 8080
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#define DEFAULT_HOST  "127.0.0.1"
#define DEFAULT_PORT  8080
#define BUF_SIZE      4096

/* ─────────────────────────────────────────────────────────
 * recv_all()
 * Reads from the socket until a "> " prompt is found,
 * then prints whatever arrived and returns.
 * This matches the server's convention of ending every
 * response with the "> " prompt string.
 * ───────────────────────────────────────────────────────── */
static void recv_all(int sock) {
    char   buf[BUF_SIZE];
    char   accum[BUF_SIZE * 8];
    int    accum_len = 0;
    ssize_t n;

    memset(accum, 0, sizeof(accum));

    while (1) {
        memset(buf, 0, sizeof(buf));
        n = recv(sock, buf, sizeof(buf) - 1, 0);
        if (n <= 0) {
            if (n == 0)       printf("\n[client] Server closed connection.\n");
            else if (n < 0)   perror("[client] recv");
            exit(0);
        }
        buf[n] = '\0';

        /* Append to accumulator (guard overflow) */
        int space = (int)sizeof(accum) - accum_len - 1;
        if (space > 0) {
            int copy = (n < space) ? (int)n : space;
            memcpy(accum + accum_len, buf, copy);
            accum_len += copy;
            accum[accum_len] = '\0';
        }

        /* Stop when the trailing prompt "> " is present */
        if (accum_len >= 2 &&
            accum[accum_len - 2] == '>' &&
            accum[accum_len - 1] == ' ') {
            /* Print everything except the trailing "> " */
            fwrite(accum, 1, accum_len - 2, stdout);
            printf("\n> ");
            fflush(stdout);
            break;
        }

        /* Also break on bare "Bye!\n" (LOGOUT/EXIT response) */
        if (strstr(accum, "Bye!") != NULL) {
            fwrite(accum, 1, accum_len, stdout);
            fflush(stdout);
            break;
        }
    }
}

/* ─────────────────────────────────────────────────────────
 * print_banner()
 * ───────────────────────────────────────────────────────── */
static void print_banner(void) {
    printf("╔══════════════════════════════════════════╗\n");
    printf("║      SMART TASK MANAGER  –  Client       ║\n");
    printf("╚══════════════════════════════════════════╝\n");
    printf("Type HELP after login for available commands.\n\n");
}

/* ─────────────────────────────────────────────────────────
 * print_local_help()
 * Printed before connection so user knows what to type.
 * ───────────────────────────────────────────────────────── */
static void print_local_help(void) {
    printf("──────────────────────────────────────────────\n");
    printf("  After login, available commands:\n");
    printf("  ADD_JOB <type> <HIGH|MEDIUM|LOW> <delay>\n");
    printf("    types: backup  report  cleanup  reminder  compile\n");
    printf("  VIEW_MY_JOBS\n");
    printf("  VIEW_ALL_JOBS\n");
    printf("  JOB_STATUS <id>\n");
    printf("  CANCEL_JOB <id>\n");
    printf("  CHANGE_PRIORITY <id> <HIGH|MEDIUM|LOW>  [ADMIN]\n");
    printf("  VIEW_LOGS                                [ADMIN]\n");
    printf("  LOGOUT   EXIT\n");
    printf("──────────────────────────────────────────────\n\n");
}

/* ─────────────────────────────────────────────────────────
 * main()
 * ───────────────────────────────────────────────────────── */
int main(int argc, char *argv[]) {
    const char *host = DEFAULT_HOST;
    int         port = DEFAULT_PORT;

    if (argc >= 2) host = argv[1];
    if (argc >= 3) port = atoi(argv[2]);

    print_banner();
    printf("[client] Connecting to %s:%d ...\n\n", host, port);

    /* ── Resolve host and connect ──────────────────────── */
    struct addrinfo hints, *res, *rp;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family   = AF_UNSPEC;      /* IPv4 or IPv6 */
    hints.ai_socktype = SOCK_STREAM;

    char port_str[8];
    snprintf(port_str, sizeof(port_str), "%d", port);

    int gai = getaddrinfo(host, port_str, &hints, &res);
    if (gai != 0) {
        fprintf(stderr, "[client] getaddrinfo: %s\n", gai_strerror(gai));
        exit(EXIT_FAILURE);
    }

    int sock = -1;
    for (rp = res; rp != NULL; rp = rp->ai_next) {
        sock = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (sock < 0) continue;
        if (connect(sock, rp->ai_addr, rp->ai_addrlen) == 0) break;
        close(sock);
        sock = -1;
    }
    freeaddrinfo(res);

    if (sock < 0) {
        fprintf(stderr,
                "[client] Cannot connect to %s:%d – is the server running?\n",
                host, port);
        exit(EXIT_FAILURE);
    }
    printf("[client] Connected.\n\n");
    print_local_help();

    /* ── Server sends welcome + LOGIN prompt first ──────── */
    recv_all(sock);

    /* ── Main I/O loop ─────────────────────────────────── */
    char input[1024];

    while (1) {
        /* Read a line from the user */
        if (fgets(input, sizeof(input), stdin) == NULL) {
            /* EOF (Ctrl-D) — send EXIT gracefully */
            send(sock, "EXIT\n", 5, 0);
            recv_all(sock);
            break;
        }

        /* Strip trailing newline */
        size_t len = strlen(input);
        while (len > 0 && (input[len - 1] == '\n' || input[len - 1] == '\r'))
            input[--len] = '\0';

        if (len == 0) continue;   /* blank line – skip */

        /* Send command to server with newline terminator */
        char to_send[1030];
        snprintf(to_send, sizeof(to_send), "%s\n", input);
        if (send(sock, to_send, strlen(to_send), 0) < 0) {
            perror("[client] send");
            break;
        }

        /* Receive and display server response */
        recv_all(sock);

        /* Local exit after LOGOUT/EXIT */
    char upper[32];
strncpy(upper, input, 31);
upper[31] = '\0';

/* Convert to uppercase */
for (int i = 0; upper[i]; i++) {
    if (upper[i] >= 'a' && upper[i] <= 'z') {
        upper[i] -= 32;
    }
}

/* Handle commands */
if (strncmp(upper, "EXIT", 4) == 0) {
    printf("[client] Disconnected.\n");
    break;
}

if (strncmp(upper, "LOGOUT", 6) == 0) {
    printf("[client] Logged out. You may log in again.\n");
    continue;
}
       
    }

    close(sock);
    return 0;
}
