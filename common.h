#ifndef COMMON_H
#define COMMON_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <semaphore.h>
#include <fcntl.h>
#include <errno.h>
#include <time.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <arpa/inet.h>

/* ─── Constants ─────────────────────────────────────────── */
#define PORT            8080
#define MAX_CLIENTS     16
#define MAX_JOBS        256
#define MAX_WORKERS     4          /* semaphore limit on parallel workers */
#define BUF_SIZE        1024
#define MAX_USERNAME    32
#define MAX_PASSWORD    32
#define MAX_JOB_TYPE    32

#define USERS_FILE      "users.txt"
#define JOBS_FILE       "jobs.txt"
#define LOGS_FILE       "logs.txt"

/* ─── Enums ──────────────────────────────────────────────── */
typedef enum {
    ROLE_ADMIN = 0,
    ROLE_USER,
    ROLE_GUEST
} Role;

typedef enum {
    PRIO_HIGH = 0,
    PRIO_MEDIUM,
    PRIO_LOW
} Priority;

typedef enum {
    STATE_PENDING = 0,
    STATE_RUNNING,
    STATE_COMPLETED,
    STATE_FAILED,
    STATE_CANCELLED
} JobState;

/* ─── Structs ────────────────────────────────────────────── */
typedef struct {
    char     username[MAX_USERNAME];
    char     password[MAX_PASSWORD];
    Role     role;
} User;

typedef struct {
    int       id;
    char      owner[MAX_USERNAME];   /* username who submitted */
    char      type[MAX_JOB_TYPE];    /* backup / report / cleanup … */
    Priority  priority;
    JobState  state;
    int       delay;                 /* seconds before execution */
    time_t    submit_time;
    time_t    start_time;
    pid_t     worker_pid;            /* PID of child process */
} Job;

/* ─── Globals (defined in server.c, extern elsewhere) ───── */
extern Job            job_queue[MAX_JOBS];
extern int            job_count;
extern pthread_mutex_t queue_mutex;
extern sem_t           worker_sem;

/* ─── storage.c ──────────────────────────────────────────── */
int   lock_file(int fd, short type);
int   unlock_file(int fd);
int   load_users(User *users, int max);
int   load_jobs(void);
void  save_job(const Job *j);
void  update_job_status(int job_id, JobState state);
void  write_log(const char *username, const char *action);
void  view_logs(int sock);

/* ─── scheduler.c ────────────────────────────────────────── */
int   add_job(const char *owner, const char *type, Priority prio, int delay);
void *schedule_loop(void *arg);
void  spawn_worker(Job *j);
void  execute_job(Job *j);           /* runs inside child process */
int   cancel_job(int id, const char *requester, Role role);
int   change_priority(int id, Priority new_prio, Role role);
void  view_jobs(int sock, const char *requester, Role role);
void  view_my_jobs(int sock, const char *requester);
void  job_status(int sock, int id, const char *requester, Role role);

/* ─── Helpers ────────────────────────────────────────────── */
static inline const char *role_str(Role r) {
    switch (r) {
        case ROLE_ADMIN: return "ADMIN";
        case ROLE_USER:  return "USER";
        default:         return "GUEST";
    }
}
static inline const char *prio_str(Priority p) {
    switch (p) {
        case PRIO_HIGH:   return "HIGH";
        case PRIO_MEDIUM: return "MEDIUM";
        default:          return "LOW";
    }
}
static inline const char *state_str(JobState s) {
    switch (s) {
        case STATE_PENDING:   return "PENDING";
        case STATE_RUNNING:   return "RUNNING";
        case STATE_COMPLETED: return "COMPLETED";
        case STATE_FAILED:    return "FAILED";
        default:              return "CANCELLED";
    }
}
static inline Priority prio_from_str(const char *s) {
    if (strcasecmp(s, "HIGH")   == 0) return PRIO_HIGH;
    if (strcasecmp(s, "MEDIUM") == 0) return PRIO_MEDIUM;
    return PRIO_LOW;
}

#endif /* COMMON_H */
