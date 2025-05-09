# ECE670 Project

An ultra-fast, highly robust FUSE-based caching filesystem for HTTP and file:// backends, featuring an LRU eviction policy augmented with “hotness” tracking to keep your most frequently accessed files in cache, delivering exceptional performance and reliability when serving remote resources locally.

## Directory Structure

```
ECE670_Project
├── backend
│   ├── backend.h
│   ├── downloaded_file.txt
│   ├── http_backend.cc
│   ├── instructions.txt
│   ├── local_server.py
│   └── test_data
│       ├── test_1000kb.txt
│       ├── test_100kb.txt
│       ├── test_10kb.txt
│       ├── test_1kb.txt
│       └── test_dir
│           ├── file_0.txt
│           ├── file_1.txt
│           ├── file_2.txt
│           ├── file_3.txt
│           ├── file_4.txt
│           └── smoke.txt
├── cache
│   ├── block_store.cc
│   ├── block_store.h
│   ├── cache_manager.cc
│   ├── cache_manager.h
│   ├── fs_layout.h
│   ├── legacy_shims.cc
│   ├── policy
│   │   ├── lru_policy.cc
│   │   ├── lru_policy.h
│   │   ├── metadata
│   │   │   ├── metadata_store.cc
│   │   │   └── metadata_store.h
│   │   ├── time_policy.cc
│   │   └── time_policy.h
│   ├── thread_pool.cc
│   ├── thread_pool.h
│   └── thread_pool.inl
├── cache_meta.db
├── fuse
│   ├── data-dir
│   │   └── test
│   │       └── hello.txt
│   ├── fuse.cc
│   ├── fuse_common.h
│   ├── fuse.h
│   ├── fuse_log.h
│   ├── fuse_ops.cc
│   ├── fuse_ops.h
│   ├── fuse_opt.h
│   ├── fusexec
│   └── test_fuse
│       └── test
│           └── foo.txt
├── main.cc
├── Makefile
├── mnt
├── README.md
├── test_cache.cc
├── test_eviction.cc
├── test_fuse.sh
├── test_http.cc
└── test_read.cc
```

## Overview

This project implements a FUSE filesystem that transparently caches remote files served over HTTP (or accessed via `file://`). It consists of:

- **backend/**: HTTP client and local server for testing.
- **cache/**: Core caching layer with pluggable policies.
- **fuse/**: FUSE callbacks and operations integrating cache with remote backends.
- **main.cc & Makefile**: Build the command-line mounting tool.
- **tests**: Unit and integration tests for cache, eviction, HTTP, and FUSE.

## Caching Architecture

1. **Cache Manager** (`cache/cache_manager.*`):
   - Coordinates reading/writing through `block_store`.
   - Tracks metadata in `cache_meta.db`.
   - Evicts entries when the cache directory exceeds timeouts or policy limits.

2. **Block Store** (`cache/block_store.*`):
   - Organizes cached file data into fixed-size blocks.
   - Supports random-access reads/writes for efficient partial updates.

3. **Eviction Policies** (`cache/policy/`):
   - **LRU** (`lru_policy.*`): Least-Recently-Used eviction.
   - **Time-based** (`time_policy.*`): Evict entries older than configured TTL.
   - Metadata persistence in `metadata_store.*`.

4. **Thread Pool** (`cache/thread_pool.*`):
   - Executes background eviction and I/O without blocking FUSE threads.

5. **FUSE Integration** (`fuse/fuse.cc`, `fuse_ops.*`):
   - **`getattr`**: Checks cache metadata or queries remote `/api/info`.
   - **`read`/`write`**: Streams data through `cache_manager`, falling back to `data_backend`.
   - **Directory listing**: Uses `/api/list` to parse JSON names and local cache entries.
   - **Cache eviction**: Triggered on `release` of file handles to maintain cache health.

This layered design ensures:
- **Transparency**: Applications access remote files as if they were local.
- **Performance**: Frequently accessed data served from local disk.
- **Flexibility**: Swap eviction policies or backends without touching FUSE logic.

## Building & Running

### Dependencies

- GCC or Clang with C++17 support
- libfuse3 (`fuse3` development headers)
- libcurl (`curl` development headers)
- CMake or GNU Make

### Build

```bash
git clone https://github.com/VanshK123/ECE670_Project.git
mkdir ECE670_Project
make
```

### Mounting

```bash
mkdir /tmp/mnt
./fusexec <cache_dir> http://localhost:8000 /tmp/mnt
```

### Testing

- **Cache unit tests**:
  ```bash
  make test_cache
  ```
- **Eviction tests**:
  ```bash
  make test_eviction
  ```
- **HTTP backend tests**:
  ```bash
  make test_http
  ```
- **FUSE integration**:
  ```bash
  ./test_fuse.sh
  ```
