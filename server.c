/*
 * server.c
 * Main server process:
 *   - TCP socket, accepts clients
 *   - One pthread per client (handle_client)
 *   - One scheduler pthread (schedule_loop)
 *   - Role-based command dispatch
 *   - Semaphore limits concurrent worker processes
 */

#include "common.h"

/* ─── Static user table loaded at startup ────────────────── */
#define MAX_USERS 64
static User users[MAX_USERS];
static int  user_count = 0;

/* ─────────────────────────────────────────────────────────
 * authenticate()
 * Returns pointer to User on success, NULL on failure.
 * ───────────────────────────────────────────────────────── */
static const User *authenticate(const char *uname, const char *pass) {
    for (int i = 0; i < user_count; i++) {
        if (strcmp(users[i].username, uname) == 0 &&
            strcmp(users[i].password, pass)  == 0) {
            return &users[i];
        }
    }
    return NULL;
}

/* ─────────────────────────────────────────────────────────
 * send_msg() – helper to send a string to client
 * ───────────────────────────────────────────────────────── */
static void send_msg(int sock, const char *msg) {
    send(sock, msg, strlen(msg), 0);
}

/* ─────────────────────────────────────────────────────────
 * handle_client()
 * Runs in its own pthread.
 * Protocol: line-oriented text commands.
 * ───────────────────────────────────────────────────────── */
typedef struct {
    int  sock;
    char addr[INET_ADDRSTRLEN];
} ClientArgs;

static void *handle_client(void *arg) {
    ClientArgs *ca   = (ClientArgs *)arg;
    int         sock = ca->sock;
    char        addr[INET_ADDRSTRLEN];
    strncpy(addr, ca->addr, INET_ADDRSTRLEN);
    free(ca);

    pthread_detach(pthread_self());

    char buf[BUF_SIZE];
    char response[BUF_SIZE * 2];

    int active = 1;

    /* ── OUTER SESSION LOOP ── */
    while (active) {

        send_msg(sock, "Welcome to Smart Task Manager\nLOGIN <username> <password>\n> ");

        const User *logged_in = NULL;
        int attempts = 0;

        /* ── LOGIN LOOP ── */
        while (!logged_in && attempts < 3) {
            int n = recv(sock, buf, sizeof(buf) - 1, 0);
            if (n <= 0) {
                close(sock);
                return NULL;
            }

            buf[n] = '\0';
            buf[strcspn(buf, "\r\n")] = '\0';

            char cmd[16], uname[MAX_USERNAME], pass[MAX_PASSWORD];

            if (sscanf(buf, "%15s %31s %31s", cmd, uname, pass) == 3 &&
                strcasecmp(cmd, "LOGIN") == 0)
            {
                logged_in = authenticate(uname, pass);

                if (logged_in) {
                    snprintf(response, sizeof(response),
                             "OK: Logged in as %s [%s]\n> ",
                             logged_in->username, role_str(logged_in->role));
                    send_msg(sock, response);
                    write_log(logged_in->username, "LOGIN");
                } else {
                    attempts++;
                    snprintf(response, sizeof(response),
                             "ERROR: Invalid credentials (%d/3)\n> ", attempts);
                    send_msg(sock, response);
                    write_log(uname, "FAILED_LOGIN");
                }
            } else {
                send_msg(sock, "ERROR: Use LOGIN <username> <password>\n> ");
            }
        }

        if (!logged_in) {
            send_msg(sock, "ERROR: Too many failed attempts. Disconnecting.\n");
            break;
        }

        /* ── COMMAND LOOP ── */
        while (1) {
            int n = recv(sock, buf, sizeof(buf) - 1, 0);

            if (n <= 0) {
                write_log(logged_in->username, "DISCONNECT");
                active = 0;
                break;
            }

            buf[n] = '\0';
            buf[strcspn(buf, "\r\n")] = '\0';

            if (strlen(buf) == 0) {
                send_msg(sock, "> ");
                continue;
            }

            /* Tokenise */
            char *tokens[8];
            int   tc = 0;

            char buf_copy[BUF_SIZE];
            strncpy(buf_copy, buf, BUF_SIZE - 1);

            char *tok = strtok(buf_copy, " ");
            while (tok && tc < 8) {
                tokens[tc++] = tok;
                tok = strtok(NULL, " ");
            }

            if (tc == 0) {
                send_msg(sock, "> ");
                continue;
            }

            const char *cmd = tokens[0];

            /* ── LOGOUT ── */
            if (strcasecmp(cmd, "LOGOUT") == 0) {
                write_log(logged_in->username, "LOGOUT");
                send_msg(sock, "Logged out.\nLOGIN <username> <password>\n> ");
                break;   /* go back to login */
            }

            /* ── EXIT ── */
            if (strcasecmp(cmd, "EXIT") == 0) {
                write_log(logged_in->username, "EXIT");
                send_msg(sock, "Bye!\n");
                active = 0;
                break;
            }

            /* ── ADD_JOB ── */
            if (strcasecmp(cmd, "ADD_JOB") == 0) {
                if (logged_in->role == ROLE_GUEST) {
                    send_msg(sock, "ERROR: Guests cannot submit jobs.\n> ");
                    continue;
                }

                if (tc < 4) {
                    send_msg(sock, "ERROR: Usage: ADD_JOB <type> <priority> <delay>\n> ");
                    continue;
                }

                const char *type  = tokens[1];
                Priority    prio  = prio_from_str(tokens[2]);
                int         delay = atoi(tokens[3]);
                if (delay < 0) delay = 0;

                int jid = add_job(logged_in->username, type, prio, delay);

                if (jid < 0) {
                    send_msg(sock, "ERROR: Queue is full.\n> ");
                } else {
                    snprintf(response, sizeof(response),
                             "OK: Job %d submitted (%s %s delay=%ds)\n> ",
                             jid, type, prio_str(prio), delay);
                    send_msg(sock, response);
                }
                continue;
            }

            /* ── VIEW_MY_JOBS ── */
            if (strcasecmp(cmd, "VIEW_MY_JOBS") == 0) {
                view_my_jobs(sock, logged_in->username);
                send_msg(sock, "> ");
                continue;
            }

            /* ── JOB_STATUS ── */
            if (strcasecmp(cmd, "JOB_STATUS") == 0) {
                if (tc < 2) {
                    send_msg(sock, "ERROR: Usage: JOB_STATUS <id>\n> ");
                    continue;
                }

                int id = atoi(tokens[1]);
                job_status(sock, id, logged_in->username, logged_in->role);
                send_msg(sock, "> ");
                continue;
            }

            /* ── VIEW_ALL_JOBS ── */
            if (strcasecmp(cmd, "VIEW_ALL_JOBS") == 0) {
                if (logged_in->role == ROLE_GUEST) {
                    send_msg(sock, "ERROR: Permission denied.\n> ");
                    continue;
                }
                view_jobs(sock, logged_in->username, logged_in->role);
                send_msg(sock, "> ");
                continue;
            }

            /* ── CANCEL_JOB ── */
            if (strcasecmp(cmd, "CANCEL_JOB") == 0) {
                if (tc < 2) {
                    send_msg(sock, "ERROR: Usage: CANCEL_JOB <id>\n> ");
                    continue;
                }
                int id = atoi(tokens[1]);
                int rc = cancel_job(id, logged_in->username, logged_in->role);
                if (rc == 0) {
                    snprintf(response, sizeof(response),
                             "OK: Job %d cancelled.\n> ", id);
                } else if (rc == -2) {
                    snprintf(response, sizeof(response),
                             "ERROR: Permission denied for job %d.\n> ", id);
                } else if (rc == -3) {
                    snprintf(response, sizeof(response),
                             "ERROR: Job %d is already in a terminal state.\n> ", id);
                } else {
                    snprintf(response, sizeof(response),
                             "ERROR: Job %d not found.\n> ", id);
                }
                send_msg(sock, response);
                continue;
            }

            /* ── CHANGE_PRIORITY ── */
            if (strcasecmp(cmd, "CHANGE_PRIORITY") == 0) {
                if (logged_in->role != ROLE_ADMIN) {
                    send_msg(sock, "ERROR: ADMIN only.\n> ");
                    continue;
                }
                if (tc < 3) {
                    send_msg(sock, "ERROR: Usage: CHANGE_PRIORITY <id> <HIGH|MEDIUM|LOW>\n> ");
                    continue;
                }
                int      id       = atoi(tokens[1]);
                Priority new_prio = prio_from_str(tokens[2]);
                int rc = change_priority(id, new_prio, logged_in->role);
                if (rc == 0) {
                    snprintf(response, sizeof(response),
                             "OK: Job %d priority changed to %s.\n> ",
                             id, prio_str(new_prio));
                } else if (rc == -2) {
                    snprintf(response, sizeof(response),
                             "ERROR: Permission denied.\n> ");
                } else {
                    snprintf(response, sizeof(response),
                             "ERROR: Job %d not found.\n> ", id);
                }
                send_msg(sock, response);
                continue;
            }

            /* ── VIEW_LOGS ── */
            if (strcasecmp(cmd, "VIEW_LOGS") == 0) {
                if (logged_in->role != ROLE_ADMIN) {
                    send_msg(sock, "ERROR: ADMIN only.\n> ");
                    continue;
                }
                view_logs(sock);
                send_msg(sock, "> ");
                continue;
            }

            /* ── HELP ── */
            if (strcasecmp(cmd, "HELP") == 0) {
                snprintf(response, sizeof(response),
                    "Commands:\n"
                    "  ADD_JOB <type> <HIGH|MEDIUM|LOW> <delay>\n"
                    "    types: backup report cleanup reminder compile\n"
                    "  VIEW_MY_JOBS\n"
                    "  VIEW_ALL_JOBS\n"
                    "  JOB_STATUS <id>\n"
                    "  CANCEL_JOB <id>\n"
                    "  CHANGE_PRIORITY <id> <HIGH|MEDIUM|LOW>  [ADMIN]\n"
                    "  VIEW_LOGS                               [ADMIN]\n"
                    "  LOGOUT   EXIT\n> ");
                send_msg(sock, response);
                continue;
            }

            /* ── DEFAULT ── */
            snprintf(response, sizeof(response),
                     "ERROR: Unknown command '%s'. Type HELP.\n> ", cmd);
            send_msg(sock, response);
        }
    }

    close(sock);
    return NULL;
}
/* ─────────────────────────────────────────────────────────
 * ensure_default_users()
 * Creates users.txt with defaults if it doesn't exist.
 * ───────────────────────────────────────────────────────── */
static void ensure_default_users(void) {
    if (access(USERS_FILE, F_OK) == 0) return;   /* already exists */
    FILE *fp = fopen(USERS_FILE, "w");
    if (!fp) { perror("create users.txt"); return; }
    fprintf(fp, "admin  admin123  ADMIN\n");
    fprintf(fp, "alice  alice123  USER\n");
    fprintf(fp, "bob    bob123    USER\n");
    fprintf(fp, "guest  guest123  GUEST\n");
    fclose(fp);
    printf("[server] Created default users.txt\n");
}

/* ─────────────────────────────────────────────────────────
 * main()
 * ───────────────────────────────────────────────────────── */
int main(void) {
   /* Ignore SIGPIPE so a disconnecting client doesn't kill server */
    signal(SIGPIPE, SIG_IGN);

    /* Leave SIGCHLD as SIG_DFL so waitpid() in reap_children() can
     * collect child exit status. SA_NOCLDWAIT must NOT be set —
     * it discards exit status before waitpid() can read it. */
    signal(SIGCHLD, SIG_DFL);

    /* Init semaphore for worker process limit */
    sem_init(&worker_sem, 0, MAX_WORKERS);

    /* Ensure data files exist */
    ensure_default_users();

    /* Load users and any persisted jobs */
    user_count = load_users(users, MAX_USERS);
    printf("[server] Loaded %d user(s).\n", user_count);

    load_jobs();
    printf("[server] Loaded %d job(s) from disk.\n", job_count);

    /* Start scheduler thread */
    pthread_t sched_tid;
    if (pthread_create(&sched_tid, NULL, schedule_loop, NULL) != 0) {
        perror("pthread_create scheduler");
        exit(EXIT_FAILURE);
    }
    printf("[server] Scheduler thread started.\n");

    /* Create TCP socket */
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) { perror("socket"); exit(EXIT_FAILURE); }

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port        = htons(PORT);

    if (bind(server_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind"); exit(EXIT_FAILURE);
    }
    if (listen(server_fd, MAX_CLIENTS) < 0) {
        perror("listen"); exit(EXIT_FAILURE);
    }

    printf("[server] Listening on port %d ...\n", PORT);

    /* Accept loop */
    while (1) {
        struct sockaddr_in client_addr;
        socklen_t          addrlen = sizeof(client_addr);
        int client_fd = accept(server_fd,
                               (struct sockaddr *)&client_addr, &addrlen);
        if (client_fd < 0) {
            if (errno == EINTR) continue;
            perror("accept");
            continue;
        }

        ClientArgs *ca = malloc(sizeof(ClientArgs));
        if (!ca) { close(client_fd); continue; }
        ca->sock = client_fd;
        inet_ntop(AF_INET, &client_addr.sin_addr, ca->addr, INET_ADDRSTRLEN);
        printf("[server] New client: %s\n", ca->addr);

        pthread_t tid;
        if (pthread_create(&tid, NULL, handle_client, ca) != 0) {
            perror("pthread_create client");
            free(ca);
            close(client_fd);
        }
    }

    sem_destroy(&worker_sem);
    close(server_fd);
    return 0;
}
