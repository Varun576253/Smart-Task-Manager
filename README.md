Smart Task Manager

Operating Systems Lab Mini Project

A multi-threaded, priority-based job scheduling system built in C that simulates a real-world job scheduler using multithreading, process management, IPC, synchronization, networking, and persistent storage.

Overview

Smart Task Manager is a Linux-based client-server application where multiple users can connect simultaneously, authenticate, and submit background jobs.

Each client is handled by a dedicated POSIX thread, while a scheduler thread continuously dispatches jobs based on priority. Jobs execute as isolated worker processes using fork(), demonstrating concurrent systems programming concepts.

How It Works
Client Workflow
User logs into the server.
Submits a job with:
Job Type
Priority
Delay
Job enters the shared priority queue.
Scheduler selects the highest-priority job.
Worker process executes the job.
Status and logs are updated automatically.
System Architecture
Clients
    │
TCP Server
    ├── Client Threads
    ├── Scheduler Thread
    │
Priority Job Queue
    │
fork() Worker Processes
    │
jobs.txt • logs.txt
Features
Multi-threaded TCP server using POSIX Threads
One dedicated thread per client
Separate scheduler thread
Priority scheduling (High → Medium → Low)
FCFS within equal priorities
Worker processes using fork()
IPC using unnamed pipes
Mutex-protected shared job queue
Semaphore-controlled worker pool
TCP socket communication
Persistent storage with fcntl locking
Role-based authentication
Activity logging
Automatic recovery after server restart
OS Concepts Demonstrated
POSIX Threads (pthreads)
Process Management (fork, waitpid)
Inter-Process Communication (Pipes)
Thread Synchronization (Mutexes & Semaphores)
TCP Socket Programming
Signal Handling
File Locking (fcntl)
Priority Scheduling
Tech Stack
Layer	Technology
Language	C (C11)
Platform	Linux
Networking	TCP Sockets
Concurrency	POSIX Threads
IPC	Pipes
Synchronization	Mutexes & Semaphores
Build	Make
Project Structure
smart_task_manager/
├── server.c
├── scheduler.c
├── storage.c
├── client.c
├── common.h
├── Makefile
├── users.txt
├── jobs.txt
└── logs.txt
Build & Run
make

# Terminal 1
./server

# Terminal 2
./client
