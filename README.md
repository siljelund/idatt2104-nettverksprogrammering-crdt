# idatt2104 2026: Conflict Free Replicated Data Types
nora har doktorgrad i read me

# crdtpp

[![Build and Test](https://github.com/siljelund/idatt2104-nettverksprogrammering-crdt/actions/workflows/ci.yml/badge.svg)](https://github.com/siljelund/idatt2104-nettverksprogrammering-crdt/actions/workflows/ci.yml)

## Introduction
A header-only C++ library implementing Conflict-free Replicated Data Types (CRDTs)
for a peer-to-peer architecture. 

## Implemented Functionality
- **G-Counter** — 
- **PN-Counter** — 
- **UDP Heartbeat** — 

## External Dependencies
| Library | Version | Purpose                                                     |
|---|---|-------------------------------------------------------------|
| [Asio](https://github.com/chriskohlhoff/asio) | 1.30.2 |                          |
| [nlohmann/json](https://github.com/nlohmann/json) | 3.11.3 |  |
| [Catch2](https://github.com/catchorg/Catch2) | 3.6.0 |                                    |

## Installation
**Requirements:**  epok nr 7

## KJØH

```bash
git clone https://github.com/siljelund/idatt2104-nettverksprogrammering-crdt.git
```
```bash
cd idatt2104-nettverksprogrammering-crdt
```
```bash
cmake -B build
```
```bash
cmake --build build
```

## Running tests
```bash
cmake --build build
```
```bash
ctest --test-dir build --output-on-failure
```