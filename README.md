# Secure Multiprocessor TCP Server

IE2102 Network Programming Assignment  
Student ID: IT24101368

## Project Overview
This project is a custom TCP client-server application developed for the IE2102 Network Programming module.

The system is implemented using:
- **C** for the TCP server
- **Python** for the client and testing scripts
- **TCP sockets**
- **Custom framing protocol**
- **Multiprocessing using `fork()`**

---

## Current Progress

### Completed
- **A1 – Custom TCP Protocol**
  - Explicit framing using:
    LEN:<n>
    <payload>
  - Valid frame handling
  - Invalid header rejection
  - Invalid length rejection
  - Oversized payload rejection
  - Partial `recv()` handling
  - Multiple messages in one buffer handling

- **A2 – Multiprocessing Design Using `fork()`**
  - Parent process keeps accepting connections
  - Child process handles one client session
  - `SIGCHLD` + `waitpid()` used for zombie cleanup
  - Multiple concurrent clients tested

### Pending
- **A3 – Authentication + Session Tokens**
- **A4 – Abuse Protection**
- **A5 – Persistent Audit Logging**

---

## Files

### Main Source Files
- `server_1368.c` → Main TCP server
- `client_1368.py` → Main Python client
- `Makefile_1368` → Build file

### Testing Files
- `client_test_1368.py` → Single-client testing script
- `multi_client_test_1368.py` → Concurrent multi-client testing script

---

## Build Instructions

Compile the server:

gcc server_1368.c -o server_1368

Or using the Makefile:

make -f Makefile_1368

---

## Run Instructions

### Start the server
./server_1368

### Run the main client
python3 client_1368.py

### Run single-client test
python3 client_test_1368.py

### Run multi-client concurrency test
python3 multi_client_test_1368.py

---

## Port and Identity Details

- **Student ID:** IT24101368
- **Server file:** `server_1368.c`
- **Client file:** `client_1368.py`
- **Port:** `50368`
- **SID:** `1013`

---

## Testing Done So Far

### A1 Tests
- Normal framed message
- Invalid header format
- Invalid length field
- Oversized payload
- Partial `recv()`
- Multiple messages in one buffer

### A2 Tests
- Multiple client connections
- Parent/child process verification
- Zombie process cleanup check
- 10-client concurrency test

---

## Notes
This repository is being developed step by step according to the assignment specification and testing requirements.
