Smart Task Manager

A multi-threaded, priority-based job scheduling system built in C that demonstrates Operating Systems concepts including multithreading, process management, synchronization, inter-process communication (IPC), networking, file locking, and persistent storage.

Overview

Smart Task Manager is a Linux-based client-server application that allows multiple users to connect simultaneously, authenticate, and submit background jobs such as backups, reports, and cleanup tasks.

Each client is handled by a dedicated POSIX thread, while a separate scheduler thread continuously monitors the shared job queue and dispatches jobs based on priority. Every scheduled job executes in an isolated worker process using fork(), enabling safe concurrent execution and demonstrating real-world job scheduling techniques.

Key Features

✅ Multi-threaded TCP server using POSIX Threads

✅ One dedicated thread for every connected client

✅ Independent scheduler thread for continuous job scheduling

✅ Priority-based scheduling (High → Medium → Low)

✅ FCFS scheduling within equal priorities

✅ Worker processes created using fork()

✅ Inter-process communication using unnamed pipes

✅ Semaphore-controlled worker pool (up to 4 concurrent workers)

✅ Thread-safe shared job queue using mutexes

✅ TCP socket-based client-server communication

✅ Persistent job storage with automatic recovery after server restart

✅ File locking using fcntl

✅ Role-based authentication (Admin, User, Guest)

✅ Activity logging and job tracking

How It Works
Client
   │
   ▼
Login & Authentication
   │
   ▼
Submit Job
(type + priority + delay)
   │
   ▼
Shared Job Queue
(Mutex Protected)
   │
   ▼
Scheduler Thread
   │
Select Highest Priority Job
   │
   ▼
Semaphore Check
(Max 4 Workers)
   │
   ▼
fork()
   │
   ▼
Worker Process
   │
Execute Job
   │
   ▼
Update Job Status
   │
   ▼
Persist to Disk & Log Activity
System Architecture
                Multiple Clients
        ┌─────────┬─────────┬─────────┐
        │         │         │
     Client     Client    Client
        │         │         │
        └─────────┴─────────┘
                  │
          TCP Socket Server
                  │
      ┌───────────┴───────────┐
      │                       │
Client Threads         Scheduler Thread
      │                       │
      └──────────┬────────────┘
                 ▼
        Shared Priority Queue
         (Mutex Protected)
                 │
      Semaphore (Max Workers)
                 │
        fork() Worker Processes
                 │
          Jobs Execute
                 │
     jobs.txt | logs.txt
Technologies Used
Component	Technology
Language	C (C11)
Platform	Linux
Networking	TCP Sockets
Concurrency	POSIX Threads
Synchronization	Mutexes & Semaphores
IPC	Pipes
Process Management	fork(), waitpid(), Signals
Storage	File-based Persistence
Build	Make
Operating Systems Concepts
POSIX Multithreading (pthreads)
Process Creation (fork)
Inter-Process Communication (Pipes)
Thread Synchronization (Mutexes)
Semaphore-based Resource Management
TCP Socket Programming
Signal Handling (SIGTERM)
File Locking (fcntl)
Non-blocking Process Reaping (waitpid)
Priority Scheduling
Concurrent Resource Management
Project Structure
smart_task_manager/
│
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
make

# Terminal 1
./server

# Terminal 2
./client
Sample Workflow
User logs into the server.
Client submits a job with its type, priority, and delay.
The scheduler inserts the job into the shared priority queue.
The scheduler selects the highest-priority pending job.
A worker process is created using fork().
The worker executes the job independently.
Job status and logs are updated and persisted.
Pending jobs are automatically restored after a server restart.
Learning Outcomes

This project demonstrates practical implementation of:

Concurrent systems programming
Multi-threaded server design
Process scheduling
Synchronization primitives
Process lifecycle management
Client-server networking
Persistent storage
Operating Systems concepts in C
