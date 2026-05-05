/*
 * scheduler.c
 * Priority scheduler (HIGH > MEDIUM > LOW), FCFS tie-break.
 * Dispatcher runs in its own pthread; workers are forked child processes.
 * IPC: unnamed pipe between scheduler and each worker.
 * Signals: SIGTERM to cancel a running job.
 */

#include "common.h"
#include <sys/wait.h>

/* ─── Global definitions (declared extern in common.h) ──── */
Job             job_queue[MAX_JOBS];
int             job_count = 0;
pthread_mutex_t queue_mutex  = PTHREAD_MUTEX_INITIALIZER;
sem_t           worker_sem;                /* initialised in server.c */

static int next_id = 1;   /* auto-increment job ID */

/* ─────────────────────────────────────────────────────────
 * add_job()
 * Inserts a new PENDING job; returns job id or -1 on error.
 * ───────────────────────────────────────────────────────── */
int add_job(const char *owner, const char *type, Priority prio, int delay) {
    pthread_mutex_lock(&queue_mutex);

    if (job_count >= MAX_JOBS) {
        pthread_mutex_unlock(&queue_mutex);
        return -1;   /* queue full */
    }

    /* Duplicate ID guard (shouldn't happen with auto-increment, but be safe) */
    for (int i = 0; i < job_count; i++) {
        if (job_queue[i].id == next_id) { next_id++; }
    }

    Job j;
    memset(&j, 0, sizeof(j));
    j.id          = next_id++;
    j.priority    = prio;
    j.state       = STATE_PENDING;
    j.delay       = delay;
    j.submit_time = time(NULL);
    j.start_time  = 0;
    j.worker_pid  = 0;
    strncpy(j.owner, owner, MAX_USERNAME - 1);
    strncpy(j.type,  type,  MAX_JOB_TYPE  - 1);

    job_queue[job_count++] = j;

    /* Persist immediately */
    save_job(&job_queue[job_count - 1]);

    pthread_mutex_unlock(&queue_mutex);
    write_log(owner, type);
    return j.id;
}

/* ─────────────────────────────────────────────────────────
 * pick_next_job()  [internal]
 * Returns index of highest-priority PENDING job (FCFS tie-break).
 * Caller must hold queue_mutex.
 * ───────────────────────────────────────────────────────── */
static int pick_next_job(void) {
    int best = -1;
    for (int i = 0; i < job_count; i++) {
        if (job_queue[i].state != STATE_PENDING) continue;
        if (best == -1) { best = i; continue; }

        /* Lower enum value = higher priority */
        if (job_queue[i].priority < job_queue[best].priority) {
            best = i;
        } else if (job_queue[i].priority == job_queue[best].priority &&
                   job_queue[i].submit_time < job_queue[best].submit_time) {
            /* FCFS tie-break */
            best = i;
        }
    }
    return best;
}

/* ─────────────────────────────────────────────────────────
 * execute_job()
 * Runs inside the child (worker) process.
 * Receives job type from parent via pipe read-end fd.
 * ───────────────────────────────────────────────────────── */
void execute_job(Job *j) {
    /* Honour the delay before execution */
    if (j->delay > 0) sleep(j->delay);

    /* Simulate job work based on type */
    if (strcmp(j->type, "backup") == 0) {
        /* Simulate backup: copy /etc/hostname to /tmp/backup_<id> */
        char cmd[256];
        snprintf(cmd, sizeof(cmd),
                 "cp /etc/hostname /tmp/stm_backup_%d 2>/dev/null", j->id);
        system(cmd);

    } else if (strcmp(j->type, "report") == 0) {
        /* Write a small report file */
        char path[64];
        snprintf(path, sizeof(path), "/tmp/stm_report_%d.txt", j->id);
        FILE *fp = fopen(path, "w");
        if (fp) {
            fprintf(fp, "Report for job %d\nOwner: %s\nTime: %ld\n",
                    j->id, j->owner, (long)time(NULL));
            fclose(fp);
        }

    } else if (strcmp(j->type, "cleanup") == 0) {
        /* Remove own temp files */
        char cmd[128];
        snprintf(cmd, sizeof(cmd), "rm -f /tmp/stm_* 2>/dev/null");
        system(cmd);

    } else if (strcmp(j->type, "reminder") == 0) {
        /* Just log the reminder */
        write_log(j->owner, "REMINDER triggered");

    } else if (strcmp(j->type, "compile") == 0) {
        /* Try to compile a dummy file if present */
        system("gcc --version > /tmp/stm_compile_check.txt 2>&1");

    } else {
        /* Generic: safe echo */
        char cmd[128];
        snprintf(cmd, sizeof(cmd),
                 "echo 'Job %d (%s) executed' >> /tmp/stm_generic.log",
                 j->id, j->type);
        system(cmd);
    }

    /* Exit code signals success to parent */
    printf("[worker] Job %d finished execution\n", j->id);
    _exit(EXIT_SUCCESS);
}

/* ─────────────────────────────────────────────────────────
 * spawn_worker()
 * Fork a child process; use a pipe for IPC (send job type).
 * Semaphore limits MAX_WORKERS parallel children.
 * ───────────────────────────────────────────────────────── */
void spawn_worker(Job *j) {
    int pipefd[2];
    if (pipe(pipefd) < 0) { perror("pipe"); return; }

    sem_wait(&worker_sem);   /* block if MAX_WORKERS already running */

    pid_t pid = fork();
    if (pid < 0) {
        perror("fork");
        sem_post(&worker_sem);
        close(pipefd[0]); close(pipefd[1]);
        return;
    }

    if (pid == 0) {
        /* ── Child ─────────────────────────────────────── */
        close(pipefd[1]);   /* close write end */

        /* Read job type from parent (IPC via pipe) */
        char buf[MAX_JOB_TYPE];
        ssize_t n = read(pipefd[0], buf, sizeof(buf) - 1);
        if (n > 0) { buf[n] = '\0'; strncpy(j->type, buf, MAX_JOB_TYPE - 1); }
        close(pipefd[0]);

        execute_job(j);
        /* execute_job calls _exit() */

    } else {
        /* ── Parent (scheduler thread) ─────────────────── */
        close(pipefd[0]);   /* close read end */

        /* Send job type to child via pipe */
        write(pipefd[1], j->type, strlen(j->type));
        close(pipefd[1]);

        /* Update state */
        pthread_mutex_lock(&queue_mutex);
        for (int i = 0; i < job_count; i++) {
            if (job_queue[i].id == j->id) {
                job_queue[i].state      = STATE_RUNNING;
                job_queue[i].worker_pid = pid;
                job_queue[i].start_time = time(NULL);
                break;
            }
        }
        update_job_status(j->id, STATE_RUNNING);
        pthread_mutex_unlock(&queue_mutex);

        char log_buf[64];
        snprintf(log_buf, sizeof(log_buf), "JOB %d STARTED (pid=%d)", j->id, pid);
        write_log(j->owner, log_buf);

        /* Wait in a detached helper so scheduler loop doesn't block */
        /* We use a quick non-blocking SIGCHLD model: check in loop */
        (void)pid;   /* handled by waitpid in schedule_loop */
    }
}

/* ─────────────────────────────────────────────────────────
 * reap_children()  [internal]
 * Non-blocking waitpid to collect finished workers.
 * Called every iteration of schedule_loop.
 * ───────────────────────────────────────────────────────── */
static void reap_children(void) {
    int   status;
    pid_t pid;

    while (1) {
        pid = waitpid(-1, &status, WNOHANG);
        if (pid <= 0) break;   /* no more finished children */

        JobState final = (WIFEXITED(status) && WEXITSTATUS(status) == 0)
                         ? STATE_COMPLETED : STATE_FAILED;

        printf("[scheduler] Worker %d finished → %s\n", pid, state_str(final));
        fflush(stdout);

        pthread_mutex_lock(&queue_mutex);
        for (int i = 0; i < job_count; i++) {
            if (job_queue[i].worker_pid == pid &&
                job_queue[i].state == STATE_RUNNING)
            {
                update_job_status(job_queue[i].id, final);

                char log_buf[64];
                snprintf(log_buf, sizeof(log_buf), "JOB %d %s",
                         job_queue[i].id, state_str(final));
                write_log(job_queue[i].owner, log_buf);
                break;
            }
        }
        pthread_mutex_unlock(&queue_mutex);
        sem_post(&worker_sem);   /* free worker slot for next dispatch */
    }
}
/* ─────────────────────────────────────────────────────────
 * schedule_loop()
 * Runs as a dedicated pthread inside the server process.
 * Polls queue, dispatches jobs, reaps finished workers.
 * ───────────────────────────────────────────────────────── */
void *schedule_loop(void *arg) {
    (void)arg;
    while (1) {
        reap_children();

        pthread_mutex_lock(&queue_mutex);
        int idx = pick_next_job();
        if (idx >= 0) {
            Job *j = &job_queue[idx];
            /* Mark tentatively so another iteration won't pick it again */
            // j->state = STATE_RUNNING;
            pthread_mutex_unlock(&queue_mutex);
            spawn_worker(j);
        } else {
            pthread_mutex_unlock(&queue_mutex);
        }

        sleep(1);   /* poll interval */
    }
    return NULL;
}

/* ─────────────────────────────────────────────────────────
 * cancel_job()
 * ADMIN: any job.  USER: only own PENDING jobs.
 * Returns 0 on success, -1 on failure.
 * ───────────────────────────────────────────────────────── */
int cancel_job(int id, const char *requester, Role role) {
    pthread_mutex_lock(&queue_mutex);

    for (int i = 0; i < job_count; i++) {
        if (job_queue[i].id != id) continue;

        Job *j = &job_queue[i];

        /* Permission check */
        if (role != ROLE_ADMIN &&
            strcmp(j->owner, requester) != 0) {
            pthread_mutex_unlock(&queue_mutex);
            return -2;   /* unauthorised */
        }

        if (j->state == STATE_CANCELLED ||
            j->state == STATE_COMPLETED ||
            j->state == STATE_FAILED) {
            pthread_mutex_unlock(&queue_mutex);
            return -3;   /* already terminal */
        }

        /* Kill running worker */
        if (j->state == STATE_RUNNING && j->worker_pid > 0) {
            kill(j->worker_pid, SIGTERM);
        }

        j->state = STATE_CANCELLED;   /* update in-memory while holding lock */
        update_job_status(id, STATE_CANCELLED);
        pthread_mutex_unlock(&queue_mutex);

        char log_buf[64];
        snprintf(log_buf, sizeof(log_buf), "CANCEL JOB %d", id);
        write_log(requester, log_buf);
        return 0;
    }

    pthread_mutex_unlock(&queue_mutex);
    return -1;   /* not found */
}

/* ─────────────────────────────────────────────────────────
 * change_priority()
 * ADMIN only; returns 0 on success.
 * ───────────────────────────────────────────────────────── */
int change_priority(int id, Priority new_prio, Role role) {
    if (role != ROLE_ADMIN) return -2;

    pthread_mutex_lock(&queue_mutex);
    for (int i = 0; i < job_count; i++) {
        if (job_queue[i].id == id) {
            job_queue[i].priority = new_prio;
            update_job_status(id, job_queue[i].state);   /* flush file */
            pthread_mutex_unlock(&queue_mutex);

            char log_buf[64];
            snprintf(log_buf, sizeof(log_buf),
                     "CHANGE_PRIORITY JOB %d -> %s", id, prio_str(new_prio));
            write_log("admin", log_buf);
            return 0;
        }
    }
    pthread_mutex_unlock(&queue_mutex);
    return -1;
}

/* ─────────────────────────────────────────────────────────
 * view_jobs()
 * ADMIN: all jobs.  USER: own jobs.  GUEST: denied.
 * Sends formatted list over the client socket.
 * ───────────────────────────────────────────────────────── */
void view_jobs(int sock, const char *requester, Role role) {
    if (role == ROLE_GUEST) {
        send(sock, "ERROR: Guests may only view queue stats.\n", 42, 0);
        return;
    }

    char buf[BUF_SIZE * 4];
    int  pos = 0;
    pos += snprintf(buf + pos, sizeof(buf) - pos,
                    "%-5s %-12s %-10s %-8s %-10s %-6s\n",
                    "ID", "OWNER", "TYPE", "PRIO", "STATE", "DELAY");
    pos += snprintf(buf + pos, sizeof(buf) - pos,
                    "%-5s %-12s %-10s %-8s %-10s %-6s\n",
                    "---", "-------", "----", "----", "-----", "-----");

    pthread_mutex_lock(&queue_mutex);
    for (int i = 0; i < job_count; i++) {
        const Job *j = &job_queue[i];
        if (role == ROLE_USER && strcmp(j->owner, requester) != 0) continue;
        pos += snprintf(buf + pos, sizeof(buf) - pos,
                        "%-5d %-12s %-10s %-8s %-10s %-6d\n",
                        j->id, j->owner, j->type,
                        prio_str(j->priority), state_str(j->state), j->delay);
        if (pos > (int)sizeof(buf) - 128) break;   /* prevent overflow */
    }
    pthread_mutex_unlock(&queue_mutex);

    send(sock, buf, strlen(buf), 0);
}

/* ─────────────────────────────────────────────────────────
 * view_my_jobs()  – USER convenience wrapper
 * ───────────────────────────────────────────────────────── */
void view_my_jobs(int sock, const char *requester) {
    view_jobs(sock, requester, ROLE_USER);
}

/* ─────────────────────────────────────────────────────────
 * job_status()
 * Returns status of a single job.
 * ───────────────────────────────────────────────────────── */
void job_status(int sock, int id, const char *requester, Role role) {
    char buf[256];
    pthread_mutex_lock(&queue_mutex);
    for (int i = 0; i < job_count; i++) {
        const Job *j = &job_queue[i];
        if (j->id != id) continue;

        /* Access control */
        if (role == ROLE_USER && strcmp(j->owner, requester) != 0) {
            pthread_mutex_unlock(&queue_mutex);
            send(sock, "ERROR: Access denied.\n", 22, 0);
            return;
        }
        snprintf(buf, sizeof(buf),
                 "Job %d | owner=%s | type=%s | prio=%s | state=%s | delay=%ds\n",
                 j->id, j->owner, j->type,
                 prio_str(j->priority), state_str(j->state), j->delay);
        pthread_mutex_unlock(&queue_mutex);
        send(sock, buf, strlen(buf), 0);
        return;
    }
    pthread_mutex_unlock(&queue_mutex);
    snprintf(buf, sizeof(buf), "ERROR: Job %d not found.\n", id);
    send(sock, buf, strlen(buf), 0);
}
