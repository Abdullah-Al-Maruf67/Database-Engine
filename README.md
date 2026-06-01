# Database Engine

A high-performance, disk-based relational database engine implemented in C. This project features a custom B-Tree storage engine, a page-level persistence layer, a SQL parser, and a multi-threaded client-server architecture.

## Features

- **Custom B-Tree Implementation**: Supports indexed lookups and efficient insertions with automated node splitting and balancing.
- **Persistence Layer (Pager)**: Manages reading and writing pages to disk with a memory cache.
- **SQL Parser**: Includes a custom tokenizer and parser for basic SQL statement processing.
- **Client-Server Architecture**: Multi-threaded server utilizing POSIX threads to handle concurrent client connections via TCP sockets.
- **Concurrency Control**: Implements thread safety using mutex locks to manage access to the database catalog.
- **Variable Length Storage**: Supports variable-length row data within leaf nodes.

## Architecture

### Storage Engine
The core of the engine is a B-Tree. It distinguishes between internal nodes (for routing) and leaf nodes (for data storage). The storage is organized into fixed-size pages, managed by a Pager component that interacts with the filesystem.

### Execution Engine
Statements are processed through a pipeline:
1. **Tokenizer**: Breaks raw input strings into meaningful tokens.
2. **Parser**: Validates syntax and generates an executable statement structure.
3. **Evaluator**: Executes the parsed statement against the storage engine.

### Networking
The server listens on a specified port and spawns a new thread for every incoming connection. It uses a custom binary protocol to communicate with clients, ensuring efficient data transfer.

## Getting Started

### Prerequisites
- GCC (GNU Compiler Collection)
- Make
- POSIX-compliant environment (Linux/macOS)

### Building
To build both the server and the client, run:
```bash
make
```

### Running the Server
```bash
./db-server
```

### Running the Client
```bash
./db-client
```

## Technical Challenges Overcome
- **B-Tree Balancing**: Implementing robust splitting logic for both leaf and internal nodes to maintain tree depth.
- **Memory Management**: Managing manual memory allocation in C while ensuring no leaks during complex query execution.
- **Capture-to-Network**: Using `open_memstream` to capture standard output results into a buffer for network transmission.

## Future Enhancements
- Support for complex joins and aggregations.
- Implementation of a Write-Ahead Log (WAL) for crash recovery.
- Query optimization layer.
