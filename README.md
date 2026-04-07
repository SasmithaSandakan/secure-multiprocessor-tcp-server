# Secure Multiprocessor TCP Server

IE2102 Network Programming Assignment
Student ID: IT24101368

---

## Project Overview

This project implements a custom TCP client-server system using C and Python.
It demonstrates core networking concepts including protocol design, multiprocessing, authentication, abuse protection, and audit logging.

The system is designed and tested in a Linux (Kali) environment.

---

## Technologies Used

* C (Server Implementation)
* Python (Client & Testing Scripts)
* TCP Sockets
* OpenSSL (SHA-256 hashing)
* Linux (Kali)

---

## Assignment Implementation

### A1 – Custom TCP Protocol

Explicit framing protocol:

LEN:<n> <payload>

Features:

* Valid message parsing
* Partial `recv()` handling
* Multiple messages in one buffer
* Invalid header rejection
* Invalid length rejection
* Oversized payload rejection

---

### A2 – Multiprocessing Server

* Uses `fork()` to handle multiple clients
* Parent process accepts connections
* Child process handles each client
* Zombie processes prevented using `SIGCHLD` + `waitpid()`
* Supports concurrent client connections

---

### A3 – Authentication & Sessions

Commands:

* REGISTER <user> <pass>
* LOGIN <user> <pass>
* LOGOUT
* WHOAMI

Features:

* Salted SHA-256 password hashing
* Passwords are not stored in plaintext
* Session token generation
* Authentication required for protected commands
* Session timeout after inactivity

---

### A4 – Abuse Protection

* Per-client rate limiting
* Brute-force login protection (temporary lockout)
* Username validation (length + allowed characters)
* Payload size limit (4096 bytes)
* Internal buffer overflow protection

---

### A5 – Persistent Audit Logging

Log file:
server_IT24101368.log

Each log entry contains:

* Timestamp
* Client IP and port
* Process ID (PID)
* Username (if logged in)
* Command executed
* Result (success or failure)

Security improvement:

* Passwords in REGISTER and LOGIN are masked in logs

---

## Project Structure

server_1368.c                  - Main server
client_1368.py                - Interactive client

client_fork_test_1368.py      - Process testing (A2)
multi_client_test_1368.py     - Multi-client testing (A2)
protocol_edge_test_1368.py    - Protocol testing (A1/A4)
rate_limit_test_1368.py       - Rate limit testing (A4)

Makefile_1368                 - Build configuration
README.md                     - Project documentation
.gitignore                    - Ignored files

---

## Build Instructions

Compile manually:

gcc server_1368.c -o server_1368 -lcrypto

Or using Makefile:

make -f Makefile_1368

---

## Run Instructions

Start server:

./server_1368

Run client:

python3 client_1368.py

---

## Example Commands

REGISTER alice secret123
LOGIN alice secret123
WHOAMI
LOGOUT

---

## Testing Scripts

client_fork_test_1368.py → Fork/process behavior testing
multi_client_test_1368.py → Concurrent clients testing
protocol_edge_test_1368.py → Protocol edge cases
rate_limit_test_1368.py → Rate limiting verification

---

## Security Considerations

* Passwords stored as: username:salt:hash
* No plaintext password storage
* Sensitive data masked in logs
* Basic protection against brute-force attacks and flooding

---

## Notes

* Developed according to assignment requirements
* Tested in Linux environment
* Focused on correctness, robustness, and security awareness

---

## Author

Student ID: IT24101368
Module: IE2102 – Network Programming

