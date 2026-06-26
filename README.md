Smart Task Manager

A multi-threaded, priority-based job scheduling system built in C that demonstrates core Operating Systems concepts including concurrency, process management, synchronization, inter-process communication (IPC), networking, and persistent storage.

Overview

Smart Task Manager is a Linux-based multi-threaded client-server application where multiple users can connect simultaneously, authenticate, and submit background jobs. Each client is handled by a dedicated POSIX thread, while a separate scheduler thread continuously dispatches jobs based on priority. Jobs execute as isolated worker processes created using fork(), simulating how real-world job scheduling systems manage concurrent workloads.

Key Features
Multi-threaded TCP server using POSIX Threads (pthreads)
Dedicated thread for every connected client
Independent scheduler thread for continuous job scheduling
Priority-based scheduling (High → Medium → Low)
Thread-safe shared job queue using mutexes
Semaphore-controlled worker pool (up to 4 concurrent workers)
Worker processes created using fork()
Inter-process communication using unnamed pipes
TCP socket-based client-server communication
Persistent storage with file locking (fcntl)
Role-based authentication (Admin/User/Guest)
Activity logging and job tracking
Automatic recovery of pending jobs after server restart
Tech Stack
Component	Technology
Language	C (C11)
Platform	Linux
Networking	TCP Sockets
Concurrency	POSIX Threads (pthreads)
Synchronization	Mutexes, Semaphores
Process Management	fork(), waitpid(), Signals
IPC	Pipes
Storage	File-based Persistence
Build System	Make
System Architecture
Clients
   │
   ▼
TCP Server
   │
   ├── Client Thread (1 per client)
   ├── Client Thread
   ├── Client Thread
   └── Scheduler Thread
            │
            ▼
      Priority Job Queue
            │
            ▼
 Worker Processes (fork)
            │
            ▼
Persistent Storage (jobs.txt, logs.txt)
OS Concepts Demonstrated
Multithreading using POSIX Threads (pthreads)
Concurrent client handling
Thread synchronization using Mutexes
Semaphore-controlled resource management
Process creation using fork()
Inter-Process Communication (IPC) using Pipes
TCP Socket Programming
Signal handling (SIGTERM)
File locking using fcntl
Non-blocking process cleanup using waitpid(WNOHANG)
Priority Scheduling
Persistent file storage
Project Structure
smart_task_manager/
├── common.h
├── server.c
├── scheduler.c
├── storage.c
├── client.c
├── Makefile
├── users.txt
├── jobs.txt
└── logs.txt
Build & Run
Build
make
Start the Server
./server
Connect a Client
./client
Supported Commands
User Commands
LOGIN
LOGOUT
ADD_JOB
VIEW_MY_JOBS
VIEW_ALL_JOBS
JOB_STATUS
CANCEL_JOB
HELP
EXIT
Admin Commands
CHANGE_PRIORITY
VIEW_LOGS
Job Lifecycle
PENDING
   │
   ▼
RUNNING
   ├──► COMPLETED
   ├──► FAILED
   └──► CANCELLED

Jobs are executed in priority order (High → Medium → Low) while maintaining First-Come-First-Serve (FCFS) ordering within the same priority level.

Learning Outcomes

This project demonstrates practical implementation of:

Concurrent server architecture
Multithreaded systems programming
Process lifecycle management
Scheduling algorithms
Synchronization primitives
Client-server communication
Inter-process communication
Persistent storage management
Scalable operating system design concepts
Future Improvements
Round Robin and Shortest Job First scheduling
SQLite/PostgreSQL backend
REST API support
Web dashboard
Docker deployment
Secure TLS communication
Distributed worker nodes


