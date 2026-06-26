# Smart Task Manager

## Overview

Smart Task Manager is a Linux-based, multi-threaded job scheduling system developed as part of an **Operating Systems course project**. It simulates a real-world job scheduler where multiple clients can connect concurrently, authenticate, and submit background jobs. The system schedules jobs based on priority and demonstrates key operating system concepts, including multithreading, process management, inter-process communication, synchronization, networking, and persistent storage.

---

## Features

* Multi-threaded TCP server using POSIX Threads
* Priority-based job scheduling (High → Medium → Low)
* FCFS scheduling within the same priority level
* Concurrent client handling
* Worker process execution using `fork()`
* IPC using unnamed pipes
* Mutex and semaphore synchronization
* TCP socket communication
* Persistent job and activity logging
* Role-based user authentication

---

## Tech Stack

### Language

* C (C11)

### Platform

* Linux

### Technologies

* POSIX Threads (pthreads)
* TCP Sockets
* Pipes
* Mutexes & Semaphores
* Make

---

## Project Structure

```text
smart_task_manager/
│
├── server.c
├── client.c
├── scheduler.c
├── storage.c
├── common.h
├── Makefile
├── users.txt
├── jobs.txt
└── logs.txt
```

---

## Build & Run

```bash
make

# Terminal 1
./server

# Terminal 2
./client
```

---

## Learning Outcomes

This project demonstrates practical implementation of:

* Process Management
* Multithreading
* Inter-Process Communication (IPC)
* Thread Synchronization
* TCP Socket Programming
* Priority Scheduling
* File Locking and Persistent Storage

---

## License

This project was developed as part of an **Operating Systems course project** for educational purposes.
