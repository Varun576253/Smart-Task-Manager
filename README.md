# 🗂️ Smart Task Manager

> A multi-client, priority-based job scheduler built in C — demonstrating core OS concepts like process management, threading, IPC, file locking, and socket programming.

**Course:** EGC 301P – Operating Systems Lab Mini Project  
**Language:** C (C11) &nbsp;|&nbsp; **Platform:** Linux

---

## What Is This?

Smart Task Manager is a terminal-based client-server application where multiple users can connect over a network, log in, and submit background jobs (like backups, reports, or cleanups). The server schedules these jobs intelligently by priority, runs them as isolated child processes, and keeps a full log of everything that happens.

Think of it as a mini version of a job queue system — like how a print server or CI pipeline works — but built from scratch in C.

---

## How It Works (Architecture)

```
┌──────────┐          ┌────────────────────────────────────────────┐
│  client  │  ──TCP── │             server.c  (main process)        │
│  client  │          │                                            │
│  client  │          │   [pthread]  ←── one per connected client  │
└──────────┘          │   [pthread]  ←── scheduler loop            │
                      │       │                                    │
                      │     fork()  +  pipe (IPC)                  │
                      │   [worker] [worker] [worker]  ≤ 4 at once  │
                      └────────────────────────────────────────────┘
                         jobs.txt   users.txt   logs.txt
                         (all protected with fcntl locks)
```

Each client gets its own thread. A separate scheduler thread watches the job queue, picks the highest-priority pending job, and forks a worker process to run it. Workers communicate with the scheduler via a pipe, and at most 4 workers run at the same time (controlled by a semaphore).

---

## Project Structure

```
smart_task_manager/
├── common.h       — shared types, constants, and function declarations
├── server.c       — TCP server, authentication, command routing
├── scheduler.c    — job queue, priority dispatch, worker processes
├── storage.c      — file I/O and logging (with fcntl locking)
├── client.c       — interactive terminal client
├── Makefile
├── users.txt      — created automatically on first run
├── jobs.txt       — created automatically on first run
└── logs.txt       — created automatically on first run
```

---

## OS Concepts in Action

| Concept | Where It's Used |
|---|---|
| **Threads (pthreads)** | One thread per client + one scheduler thread |
| **Mutex** | `queue_mutex` protects the shared job queue |
| **Semaphore** | `worker_sem` limits concurrent worker processes to 4 |
| **Fork + Exec** | Each job runs in its own child process |
| **Pipes (IPC)** | Scheduler sends job type to worker via unnamed pipe |
| **Signals** | `SIGTERM` sent to worker process on `CANCEL_JOB` |
| **File Locking** | `fcntl(F_SETLKW)` read/write locks on all data files |
| **TCP Sockets** | Client-server communication over `AF_INET` |
| **Non-blocking reap** | `waitpid(WNOHANG)` collects finished workers without blocking |
| **Role-Based Access** | ADMIN / USER / GUEST — enforced per command |

---

## Getting Started

### Prerequisites

You need GCC and POSIX threads — both come standard on any Linux system.

```bash
gcc --version    # should be 5.0 or newer
```

### Build

```bash
cd smart_task_manager/
make
```

This produces two binaries: `./server` and `./client`.

### Run

You'll need **two terminal windows** open in the project directory.

**Terminal 1 — start the server:**
```bash
./server
```
```
[server] Created default users.txt
[server] Loaded 4 user(s).
[server] Loaded 0 job(s) from disk.
[server] Scheduler thread started.
[server] Listening on port 8080 ...
```

**Terminal 2 — connect a client:**
```bash
./client                      # connects to localhost:8080
./client 192.168.1.10         # custom host
./client 192.168.1.10 9090    # custom host and port
```

---

## Default Users

These are created automatically in `users.txt` the first time the server runs:

| Username | Password   | Role    |
|----------|------------|---------|
| `admin`  | `admin123` | ADMIN   |
| `alice`  | `alice123` | USER    |
| `bob`    | `bob123`   | USER    |
| `guest`  | `guest123` | GUEST   |

To add a new user, just append a line to `users.txt` and restart the server:
```
newuser  mypassword  USER
```
Valid roles: `ADMIN` &nbsp;`USER` &nbsp;`GUEST`

---

## Command Reference

### Session Commands — everyone
```
LOGIN <username> <password>
LOGOUT
EXIT
HELP
```

### Job Commands — USER and ADMIN
```
ADD_JOB <type> <priority> <delay_seconds>
```
- **type:** `backup` `report` `cleanup` `reminder` `compile`
- **priority:** `HIGH` `MEDIUM` `LOW`
- **delay:** seconds to wait before the job starts (`0` = run immediately)

```
VIEW_MY_JOBS                   — see your own jobs
VIEW_ALL_JOBS                  — USER: own jobs only; ADMIN: everyone's jobs
JOB_STATUS <id>                — detailed status of a single job
CANCEL_JOB <id>                — USER: cancel own pending jobs; ADMIN: cancel any
```

### Admin-Only Commands
```
CHANGE_PRIORITY <id> <HIGH|MEDIUM|LOW>   — reprioritise any pending job
VIEW_LOGS                                — see the full server activity log
```

---

## Job Lifecycle

```
  PENDING  ──►  RUNNING  ──►  COMPLETED
                   │
                   ├──►  FAILED
                   └──►  CANCELLED
```

- Jobs are dispatched in **priority order**: HIGH → MEDIUM → LOW
- Equal-priority jobs run in **FCFS order** (first submitted, first run)
- At most **4 jobs run in parallel** — controlled by `MAX_WORKERS` in `common.h`
- State is **saved to disk** after every transition — jobs survive a server restart
- On restart, **PENDING jobs are reloaded** automatically from `jobs.txt`

---

## Sample Session

**Logging in and submitting jobs (as alice):**
```
> LOGIN alice alice123
OK: Logged in as alice [USER]

> ADD_JOB backup HIGH 5
OK: Job 1 submitted (backup HIGH delay=5s)

> ADD_JOB report MEDIUM 10
OK: Job 2 submitted (report MEDIUM delay=10s)

> VIEW_MY_JOBS
ID    OWNER        TYPE       PRIO     STATE      DELAY
---   -------      ----       ----     -----      -----
1     alice        backup     HIGH     RUNNING    5
2     alice        report     MEDIUM   PENDING    10

> JOB_STATUS 1
Job 1 | owner=alice | type=backup | prio=HIGH | state=RUNNING | delay=5s

> CANCEL_JOB 2
OK: Job 2 cancelled.

> LOGOUT
Logged out.
```

**Admin session:**
```
> LOGIN admin admin123
OK: Logged in as admin [ADMIN]

> VIEW_ALL_JOBS
ID    OWNER        TYPE       PRIO     STATE      DELAY
---   -------      ----       ----     -----      -----
1     alice        backup     HIGH     COMPLETED  5
2     alice        report     MEDIUM   CANCELLED  10
3     bob          cleanup    LOW      PENDING    60

> CHANGE_PRIORITY 3 HIGH
OK: Job 3 priority changed to HIGH.

> VIEW_LOGS
[2025-01-15 10:22:01] USER=alice  ACTION=LOGIN
[2025-01-15 10:22:05] USER=alice  ACTION=backup
[2025-01-15 10:22:10] USER=alice  ACTION=report
[2025-01-15 10:22:41] USER=alice  ACTION=CANCEL JOB 2
...

> LOGOUT
Logged out.
```

---

## Tuning Constants

All tunable values live in `common.h`:

| Constant | Default | What It Controls |
|---|---|---|
| `PORT` | `8080` | Server listen port |
| `MAX_CLIENTS` | `16` | Max queued incoming connections |
| `MAX_JOBS` | `256` | In-memory job queue capacity |
| `MAX_WORKERS` | `4` | Max parallel worker processes |
| `BUF_SIZE` | `1024` | Socket read/write buffer size |

---

## Troubleshooting

**`bind: Address already in use`**
```bash
pkill server       # kill the old instance
# or just wait ~30 seconds for the port to be released
```

**`Cannot connect to 127.0.0.1:8080`**
```bash
# Make sure you start the server before the client
./server &
./client
```

**Jobs stuck in PENDING and never running**
- The worker semaphore may be exhausted (e.g., 4 long-running jobs are active)
- Restart the server — PENDING jobs will be reloaded and re-queued automatically

**Want to start fresh?**
```bash
make reset-data    # deletes jobs.txt and logs.txt, keeps users.txt
```

---

## Build Targets

```bash
make               # build server and client
make clean         # remove binaries and object files
make reset-data    # clear job and log data
```
#   S m a r t - T a s k - M a n a g e r  
 