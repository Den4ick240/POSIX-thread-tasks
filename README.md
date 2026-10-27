# POSIX Thread & Systems Programming Tasks

> *Educational project built during university.*

A collection of systems-level C implementations focusing on POSIX threads, manual concurrency synchronization, and a multi-threaded HTTP proxy server.

## 📦 Projects Overview

### 1. Multi-Threaded HTTP Proxy (`mtproxy`)
A concurrent HTTP proxy server written in C from scratch.
* **Thread Pool Architecture:** Implements a custom worker thread pool (`threadpool.c`) for concurrent request handling.
* **Synchronization Primitives:** Custom condition variable-based read-write locks (`condrwlock.c`) and safe task queue management.
* **HTTP Parsing:** Integrated with `picohttpparser` for lightweight, high-performance request analysis.

### 2. Concurrency Labs (`lab25`, `6`, `7`, `8`)
* Implementation of various synchronization problems, race condition mitigation, and task coordination using POSIX threads (`pthreads`) and mutexes in C.

## 🛠️ Concepts & Technologies

* **Language:** C (C99)
* **Concurrency & Systems:** POSIX Threads (`pthread`), Mutexes, Condition Variables, Thread Pools, Custom Locks
* **Networking & Protocols:** HTTP/1.1 Proxying, Socket Programming (`poll`/multiplexing)
* **Build System:** Makefiles, GCC
