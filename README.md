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
- **Salted password hashing and session-based authentication**
- **Basic abuse protection mechanisms**

---

## Assignment Progress

### Completed Parts

#### A1 – Custom TCP Protocol
Implemented explicit framing architecture using:

LEN:<n>
<payload>

Features completed:
- Valid framed message handling
- Invalid header rejection
- Invalid length rejection
- Oversized payload rejection
- Partial `recv()` handling
- Multiple messages in one buffer handling

---

#### A2 – Multiprocessing Design
Implemented concurrent TCP server using `fork()`.

Features completed:
- Parent process continues accepting new clients
- Child process handles one client session
- `SIGCHLD` + `waitpid()` used for zombie cleanup
- Multiple concurrent clients tested successfully

---

#### A3 – Authentication + Session Tokens
Implemented user authentication and session management.

Features completed:
- `REGISTER <user> <pass>`
- `LOGIN <user> <pass>`
- `LOGOUT`
- Salted password hashing using SHA-256
- Passwords not stored in plain text
- Session token generation
- Protected command support (`WHOAMI`)
- Session expiry after inactivity

---

#### A4 – Abuse Protection
Implemented basic server-side abuse protection mechanisms.

Features completed:
- Per-client rate limiting
- Failed login brute-force lockout
- Username validation
- Oversized payload rejection
- Internal receive buffer protection

---

## Pending Parts

### A5 – Persistent Audit Logging
Planned features:
- Append-only audit log file
- Timestamped event recording
- Logging of register/login/logout actions
- Logging of authentication failures and abuse events

---

## Project Files

### Main Files
- `server_1368.c` → Main TCP server implementation
- `client_1368.py` → Main interactive client
- `Makefile_1368` → Build instructions

### Testing Files
- `client_fork_test_1368.py` → A2 process/fork testing client
- `multi_client_test_1368.py` → A2 concurrent client testing
- `protocol_edge_test_1368.py` → A1/A4 protocol edge case tester
- `rate_limit_test_1368.py` → A4 rate limit testing client

### Runtime / Data Files
- `users_1368.txt` → Registered users (username + salt + hash)
- `server_1368` → Compiled server executable

---

## Build Instructions

Compile manually:

gcc server_1368.c -o server_1368 -lcrypto

Or using the Makefile:

make -f Makefile_1368

---

## Run Instructions

### Start the server
./server_1368

### Run the main client
python3 client_1368.py

### Run A2 fork/process test
python3 client_fork_test_1368.py

### Run A2 multi-client concurrency test
python3 multi_client_test_1368.py

### Run A1/A4 protocol edge tests
python3 protocol_edge_test_1368.py

### Run A4 rate limit test
python3 rate_limit_test_1368.py

---

## Port and Identity Details

- **Student ID:** IT24101368
- **Server file:** `server_1368.c`
- **Client file:** `client_1368.py`
- **Port:** `50368`
- **SID used in responses:** `1013`

---

## Testing Performed

### A1 Testing
- Normal framed message
- Partial recv
- Multiple messages in one buffer
- Invalid length field
- Oversized payload

### A2 Testing
- Parent/child process verification
- Zombie process cleanup verification
- Multi-client concurrency testing

### A3 Testing
- Successful registration
- Duplicate registration rejection
- Failed login handling
- Successful login
- Protected command access
- Logout
- Session timeout / inactivity expiry

### A4 Testing
- Invalid username rejection
- Failed login lockout
- Correct login rejection during lockout
- Login after lockout expiry
- Rate limiting
- Payload overflow rejection

---

## Security Notes
This project is an academic implementation for learning TCP protocol design, concurrency, authentication, and server-side abuse protection.

The password storage uses:
- per-user random salt
- SHA-256 hashing

For real-world production systems, stronger password hashing algorithms such as bcrypt, scrypt, Argon2, or PBKDF2 would be more appropriate.

---

## Notes
This repository is being developed step by step according to the assignment specification, Linux-based testing, and viva preparation requirements.
