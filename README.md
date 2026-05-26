# idatt2104 2026: Conflict Free Replicated Data Types

# crdtpp

[![Build and Test](https://github.com/siljelund/idatt2104-nettverksprogrammering-crdt/actions/workflows/ci.yml/badge.svg)](https://github.com/siljelund/idatt2104-nettverksprogrammering-crdt/actions/workflows/ci.yml)

## Introduction

Distributed systems face a fundamental challenge: _How do multiple nodes stay consistent when they update a shared state  at the same time?_ 

**crdtpp** explores one answer: _Data types that can be updated independently on any node, and always merge into the same result._

**crdtpp** is a proof-of-concept header-only C++ library implementing Conflict-Free Replicated Data Types (CRDTs) for a peer-to-peer architecture. The library provides five CRDT data types: G-Counter, PN-Counter, G-Set, OR-Set, and RGA. Each of the data types can be merged across replicas without coordination, keeping all nodes consistent even under concurrent updates.

The networking layer uses Asio over TCP for state sync, as well as UDP for peer tracking. Every sync message includes a Lamport clock so the order of the edits is consistent across all nodes. A terminal demo runs multiple nodes on the same machine, where they can edit a shared text document and increment a shared counter. All edits converge automatically across peers.

## Implemented Functionality
- **G-Counter** - 
- **PN-Counter** -
- **G-Set** -
- **OR-Set** -
- **UDP Heartbeat** - 
- **Terminal UI** -

## External Dependencies
| Library | Version | Purpose                                                     |
|---|---|-------------------------------------------------------------|
| [Asio](https://github.com/chriskohlhoff/asio) | 1.30.2 |                          |
| [nlohmann/json](https://github.com/nlohmann/json) | 3.11.3 |  |
| [Catch2](https://github.com/catchorg/Catch2) | 3.6.0 |                                    |

## Installation
**Requirements:** FILL THIS

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
Start the program by passing your listen port followed by any number of peer endpoints (address and port):
```bash
./build/editor_demo <your_port> [peer_address:peer_port peer_address:peer_port]
```
### Local demo (same machine, three terminals)
- For the local demo the IP-addresses are identical, which means the listening ports need to differ.

**Terminal 1:**
```bash
./build/editor_demo 9000 127.0.0.1:9001 127.0.0.1:9002
```
**Terminal 2:**
```bash
./build/editor_demo 9001 127.0.0.1:9000 127.0.0.1:9002
```
**Terminal 3:**
```bash
./build/editor_demo 9002 127.0.0.1:9000 127.0.0.1:9001
```

### Cross-machine demo (same local network)
- For a cross-machine demo the listening port can be identical for both machines, since the IP-addresses will differ. Simples approach is for all machines to use the same port, for example port `9000`.

Find your machine's local IP address:

```bash
# macOS/Linux
ifconfig | grep "inet "
```
```bash
# Windows
ipconfig
```

Then substitute `127.0.0.1` with each machine's actual IP. For example, if machine A has IP `192.168.1.5` and machine B has IP `192.168.1.10`:

**Machine A:**
```bash
./build/editor_demo 9000 192.168.1.10:9000
```
**Machine B:**
```bash
./build/editor_demo 9000 192.168.1.5:9000
```
This supports interaction for multiple machines on the same network.

### Available commands

| Command | Description |
|---|---|
| `help` | Lists all available commands |
| `status` | Shows active and inactive peer nodes |
| `insert` | Insert text into the shared buffer |
| `quit` | Exit the program |

## Running tests
```bash
cmake --build build
```
```bash
ctest --test-dir build --output-on-failure
```