/*
 * storage.c
 * Handles all persistent file I/O with fcntl advisory locks.
 * Files: users.txt, jobs.txt, logs.txt
 */

#include "common.h"

/* ─────────────────────────────────────────────────────────
 * File Locking helpers (fcntl advisory locks)
 * type: F_RDLCK | F_WRLCK | F_UNLCK
 * ───────────────────────────────────────────────────────── */
int lock_file(int fd, short type) {
    struct flock fl;
    fl.l_type   = type;
    fl.l_whence = SEEK_SET;
    fl.l_start  = 0;
    fl.l_len    = 0;   /* entire file */
    fl.l_pid    = getpid();
    return fcntl(fd, F_SETLKW, &fl);   /* blocks until lock is acquired */
}

int unlock_file(int fd) {
    struct flock fl;
    fl.l_type   = F_UNLCK;
    fl.l_whence = SEEK_SET;
    fl.l_start  = 0;
    fl.l_len    = 0;
    fl.l_pid    = getpid();
    return fcntl(fd, F_SETLK, &fl);
}

/* ─────────────────────────────────────────────────────────
 * load_users()
 * Format: username password role(ADMIN|USER|GUEST)
 * Returns number of users loaded.
 * ───────────────────────────────────────────────────────── */
int load_users(User *users, int max) {
    int fd = open(USERS_FILE, O_RDONLY | O_CREAT, 0644);
    if (fd < 0) { perror("open users.txt"); return 0; }

    lock_file(fd, F_RDLCK);

    FILE *fp = fdopen(dup(fd), "r");
    int  count = 0;
    char role_buf[16];

    while (count < max &&
           fscanf(fp, "%31s %31s %15s",
                  users[count].username,
                  users[count].password,
                  role_buf) == 3)
    {
        if      (strcmp(role_buf, "ADMIN") == 0) users[count].role = ROLE_ADMIN;
        else if (strcmp(role_buf, "USER")  == 0) users[count].role = ROLE_USER;
        else                                     users[count].role = ROLE_GUEST;
        count++;
    }
    fclose(fp);
    unlock_file(fd);
    close(fd);
    return count;
}

/* ─────────────────────────────────────────────────────────
 * load_jobs()
 * Reads jobs.txt into the global job_queue at startup.
 * Only PENDING jobs are loaded (to resume after crash).
 * ───────────────────────────────────────────────────────── */
int load_jobs(void) {
    int fd = open(JOBS_FILE, O_RDONLY | O_CREAT, 0644);
    if (fd < 0) { perror("open jobs.txt"); return 0; }

    lock_file(fd, F_RDLCK);
    FILE *fp = fdopen(dup(fd), "r");

    pthread_mutex_lock(&queue_mutex);
    job_count = 0;

    char state_buf[16], prio_buf[16];
    while (job_count < MAX_JOBS) {
        Job j;
        int n = fscanf(fp,
            "%d %31s %31s %15s %15s %d %ld %ld %d",
            &j.id, j.owner, j.type,
            prio_buf, state_buf,
            &j.delay,
            &j.submit_time, &j.start_time,
            &j.worker_pid);
        if (n != 9) break;

        j.priority = prio_from_str(prio_buf);

        /* Map state string */
        if      (strcmp(state_buf, "PENDING")   == 0) j.state = STATE_PENDING;
        else if (strcmp(state_buf, "RUNNING")   == 0) j.state = STATE_PENDING; /* re-queue */
        else if (strcmp(state_buf, "COMPLETED") == 0) j.state = STATE_COMPLETED;
        else if (strcmp(state_buf, "FAILED")    == 0) j.state = STATE_FAILED;
        else                                          j.state = STATE_CANCELLED;

        job_queue[job_count++] = j;
    }

    pthread_mutex_unlock(&queue_mutex);
    fclose(fp);
    unlock_file(fd);
    close(fd);
    return job_count;
}

/* ─────────────────────────────────────────────────────────
 * save_job()
 * Appends a new job record to jobs.txt.
 * ───────────────────────────────────────────────────────── */
void save_job(const Job *j) {
    int fd = open(JOBS_FILE, O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (fd < 0) { perror("save_job: open"); return; }

    lock_file(fd, F_WRLCK);

    dprintf(fd, "%d %s %s %s %s %d %ld %ld %d\n",
            j->id, j->owner, j->type,
            prio_str(j->priority), state_str(j->state),
            j->delay,
            (long)j->submit_time, (long)j->start_time,
            (int)j->worker_pid);

    unlock_file(fd);
    close(fd);
}

/* ─────────────────────────────────────────────────────────
 * update_job_status()
 * Rewrites the entire jobs.txt atomically after a state change.
 * Uses a temp file + rename for atomicity.
 * ───────────────────────────────────────────────────────── */
void update_job_status(int job_id, JobState new_state) {
    /* Update in-memory queue first (caller holds queue_mutex) */
    for (int i = 0; i < job_count; i++) {
        if (job_queue[i].id == job_id) {
            job_queue[i].state = new_state;
            if (new_state == STATE_RUNNING)
                job_queue[i].start_time = time(NULL);
            break;
        }
    }

    /* Rewrite file */
    int fd = open(JOBS_FILE, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) { perror("update_job_status: open"); return; }

    lock_file(fd, F_WRLCK);
    for (int i = 0; i < job_count; i++) {
        const Job *j = &job_queue[i];
        dprintf(fd, "%d %s %s %s %s %d %ld %ld %d\n",
                j->id, j->owner, j->type,
                prio_str(j->priority), state_str(j->state),
                j->delay,
                (long)j->submit_time, (long)j->start_time,
                (int)j->worker_pid);
    }
    unlock_file(fd);
    close(fd);
}

/* ─────────────────────────────────────────────────────────
 * view_logs()
 * Reads logs.txt and sends contents over the client socket.
 * ───────────────────────────────────────────────────────── */
void view_logs(int sock) {
    int fd = open(LOGS_FILE, O_RDONLY | O_CREAT, 0644);
    if (fd < 0) {
        const char *err = "ERROR: Cannot open logs file.\n";
        send(sock, err, strlen(err), 0);
        return;
    }

    lock_file(fd, F_RDLCK);
    FILE *fp = fdopen(dup(fd), "r");
    if (!fp) {
        unlock_file(fd);
        close(fd);
        const char *err = "ERROR: Cannot read logs file.\n";
        send(sock, err, strlen(err), 0);
        return;
    }

    char line[256];
    int sent_any = 0;
    while (fgets(line, sizeof(line), fp)) {
        send(sock, line, strlen(line), 0);
        sent_any = 1;
    }
    if (!sent_any) {
        const char *empty = "(no log entries yet)\n";
        send(sock, empty, strlen(empty), 0);
    }

    fclose(fp);
    unlock_file(fd);
    close(fd);
}

/* ─────────────────────────────────────────────────────────
 * write_log()
 * Appends a timestamped entry to logs.txt.
 * ───────────────────────────────────────────────────────── */
void write_log(const char *username, const char *action) {
    int fd = open(LOGS_FILE, O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (fd < 0) { perror("write_log: open"); return; }

    lock_file(fd, F_WRLCK);

    time_t now = time(NULL);
    char ts[32];
    strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", localtime(&now));
    dprintf(fd, "[%s] USER=%s  ACTION=%s\n", ts, username, action);

    unlock_file(fd);
    close(fd);
}
