# ccache-storage-redis-cpp

A [ccache remote storage helper](https://ccache.dev/storage-helpers.html) for
Redis/Redis-TLS, written in **C++**.

## Overview

This is a storage helper for [ccache] that enables caching compilation results
on Redis/Redis-TLS servers. It implements the [ccache remote storage helper
protocol].

This project aims to:

1. Provide a high-performance, production-ready Redis(s) ccache storage helper.
2. Serve as an example implementation of a ccache storage helper in **C++**.
   Feel free to use it as a starting point for implementing helpers for other
   storage service protocols.

[ccache]: https://ccache.dev
[ccache remote storage helper protocol]: https://github.com/ccache/ccache/blob/master/doc/remote_storage_helper_spec.md

## Features

- Supports Redis and Redis-TLS
- High-performance concurrent request handling
- Redis context for efficient connection reuse
- Cross-platform: Linux, macOS, Windows
- Bearer token authentication support
- Optional debug logging

## Installation

The helper should be installed in a [location where ccache searches for helper
programs]. Install it as the name `ccache-storage-redis` for Redis support and/or
`ccache-storage-rediss` for Redis-TLS support.

[location where ccache searches for helper programs]: https://github.com/ccache/ccache/blob/master/doc/manual.adoc#storage-helper-process

### Building from source

Make sure you have needed dependencies installed:

- hiredis
- [libuv](https://libuv.org)
- [CMake](https://cmake.org) 3.16+
- C++17 compiler

Clone the repository:

```bash
git clone https://github.com/ccache/ccache-storage-redis-cpp
cd ccache-storage-redis-cpp
```

To build and install with **CMake**:

```bash
cmake -B build -D CMAKE_BUILD_TYPE=Release
cmake --build build
cmake --install build
```

This will install both `ccache-storage-redis` and `ccache-storage-rediss` to the
default location (`/usr/local/bin` on Linux/Unix).
Pass `-D CMAKE_INSTALL_PREFIX=/example/dir` to CMake to install
elsewhere.

## Configuration

The helper is configured via ccache's [`remote_storage` configuration]. The
binary is automatically invoked by ccache when needed.

For example:

```bash
# Set the CCACHE_REMOTE_STORAGE environment variable:
export CCACHE_REMOTE_STORAGE="redis://cache.example.com"

# Or set remote_storage in ccache's configuration file:
ccache -o remote_storage="redis://cache.example.com"
```

[`remote_storage` configuration]: https://github.com/ccache/ccache/blob/master/doc/manual.adoc#remote-storage-backends

See also the [Redis storage wiki page] for tips on how to set up a storage server.

[Redis storage wiki page]: https://github.com/ccache/ccache/wiki/Redis-storage

### Configuration attributes

The helper supports the following custom attributes:

- `@bearer-token`: Bearer token for `Authorization` header

Example:

```bash
export CCACHE_REMOTE_STORAGE="redis://cache.example.com @header=Content-Type=application/octet-stream"
```

## Optional debug logging

You can set the `CRSH_LOGFILE` environment variable to enable debug logging to a
file:

```bash
export CRSH_LOGFILE=/path/to/debug.log
```

Note: The helper process is spawned by ccache, so the environment variable must
be set before ccache is invoked.

## Contributing

Contributions are welcome! Please submit pull requests or open issues on GitHub.
