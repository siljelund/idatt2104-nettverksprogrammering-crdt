# crdtpp — idatt2104 2026: Conflict Free Replicated Data Types

[![Build and Test](https://github.com/siljelund/idatt2104-nettverksprogrammering-crdt/actions/workflows/ci.yml/badge.svg)](https://github.com/siljelund/idatt2104-nettverksprogrammering-crdt/actions/workflows/ci.yml)

## Introduction

Distributed systems face a fundamental challenge: _How do multiple nodes stay consistent when they update a shared state  at the same time?_ 

**crdtpp** explores one answer: _Data types that can be updated independently on any node, and always merge into the same result._

**crdtpp** is a proof-of-concept header-only C++ library implementing Conflict-Free Replicated Data Types (CRDTs) for a peer-to-peer architecture. The library provides five CRDT data types: G-Counter, PN-Counter, G-Set, OR-Set, and RGA. Each of the data types can be merged across replicas without coordination, keeping all nodes consistent even under concurrent updates.

The networking layer uses Asio over TCP for state sync, as well as UDP for peer tracking. Every sync message includes a Lamport clock so the order of the edits is consistent across all nodes. A terminal demo runs multiple nodes on the same machine, where they can edit a shared text document and increment a shared counter. All edits converge automatically across peers.

## Implemented Functionality
- **G-Counter** 

A grow-only counter. Each node can only increment its own slot, and the total value is the sum across all slots. On merge, the highest value per slot is kept.
- **PN-Counter** 

A counter that supports both increment and decrement, built from two G-Counters. The value is the difference between them, and can be negative.
- **G-Set** 

A grow-only set. Elements can be added but not removed. Calling remove throws an error. Merging two G-Sets produces their union. 
- **OR-Set** 

A set that supports both add and remove. Each add attaches a unique tag to the element, so a concurrent add on another replica survives a remove. Merging takes the union of all tags.
- **RGA** 

Replicated Growable Array, used as the shared text document. Concurrent inserts at the same position are resolved deterministically using a Lamport clock and predecessor links. Deletions use tombstoning so they propagate correctly on merge.

- **Lamport Clock**

A logical clock used for causal ordering in the RGA. Every sync message carries the current clock value, so the order of edits stays consistent across all nodes even when updates arrive out of order.

- **TCP State Sync**

Peers synchronize state over persistent TCP connections. Every insert, delete, or increment triggers a full broadcast of the document and counter state to all connected peers. Incoming state is merged automatically using each CRDT's merge function.

- **UDP Heartbeat** 

Each node sends a heartbeat to its peers every 2 seconds. A peer is marked inactive after 6 seconds without one.

- **Terminal UI** 

A command line interface for interacting with the shared document and counter across multiple local nodes.

## Future Work

As a proof of concept, crdtpp demonstrates that CRDTs work in practice, but leaves several limitations.

The most noticeable limitation is that every operation sends the full document and counter state to all peers. For a small demo this is fine, but it would become slow as the document grows. A natural next step would be to send only what changed since the last sync.

There is also no persistence. Restarting a node wipes everything. Adding local storage so nodes can recover their state on restart would allow the system to survive restarts.

On the networking side, peers have to be specified manually at startup and the demo only supports localhost. Supporting peer discovery and cross-machine connections would bring it closer to a real distributed system.

**Additional limitations:**
- **ASCII only** 

RGA stores single `char` values, so multi-byte UTF-8 characters will break.
- **Fixed node count**

The number of nodes is decided at startup and cannot change while running. Node IDs are `uint8_t`, capping the system at 256 nodes.
- **No security** 

All connections are accepted without authentication and all traffic is unencrypted plaintext.
- **Unused OR-Set** 

The node maintains an `ORSet<std::string>` for tracking connected users, but it is never populated in the demo.

## External Dependencies

All dependencies are fetched automatically by CMake — no manual installation needed.

| Library | Version | Purpose                                                                                                            |
|---|---|--------------------------------------------------------------------------------------------------------------------|
| [Asio](https://github.com/chriskohlhoff/asio) | 1.30.2 | Standalone async I/O library. Handles all TCP and UDP networking - sockets, timers, and the async event loop.      |
| [nlohmann/json](https://github.com/nlohmann/json) | 3.11.3 | JSON serialization. Every CRDT implements `to_json()` / `from_json()`, and sync messages are sent as JSON over TCP. |
| [Catch2](https://github.com/catchorg/Catch2) | 3.6.0 | Testing framework used for all unit and integration tests.                     |

## Installation

### Requirements

- **C++20** compiler
- **CMake** 3.20 or newer

Clone the repository:
```bash
git clone https://github.com/siljelund/idatt2104-nettverksprogrammering-crdt.git
```
Enter the project folder:
```bash
cd idatt2104-nettverksprogrammering-crdt
```
Compile and build the project:
```bash
cmake -B build
```
```bash
cmake --build build
```

## Usage

Start a node by specifying its ID, port, and optionally a comma-separated list of peer ports:

```bash
./build/editor_demo --node <id> --port <port> [--peers <port1,port2,...>]
```

### Local demo (three terminals)

**Terminal 1:**
```bash
./build/editor_demo --node 0 --port 9000 --peers 9001,9002
```
**Terminal 2:**
```bash
./build/editor_demo --node 1 --port 9001 --peers 9000,9002
```
**Terminal 3:**
```bash
./build/editor_demo --node 2 --port 9002 --peers 9000,9001
```

### Available commands

| Command | Description |
|---|---|
| `ins <pos> <text>` | Insert text at the given position in the shared document |
| `del <pos> <len>` | Delete `len` characters starting at `pos` |
| `inc` | Increment the shared counter |
| `show` | Print the current document, counter, and peer status |
| `sync` | Manually broadcast state to all connected peers |
| `quit` | Exit the program |

## Running Tests

Make sure the project is built first, then run:

```bash
ctest --test-dir build --output-on-failure
```

The test suite covers:
- CRDT properties — commutativity, associativity, and idempotency for all data types
- OR-Set semantics — verifying that a concurrent add on one replica survives a remove on another
- RGA conflict resolution — concurrent inserts at the same position converge to the same document on all replicas
- TCP integration — two live nodes connect, sync inserts and counter increments, and converge to identical state

## API Documentation

There is no generated API documentation. Since crdtpp is a header-only library, the public API lives directly in the `include/` directory — one header per data type. The headers are the reference.