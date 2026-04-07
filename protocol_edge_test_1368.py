import socket
import time

HOST = "127.0.0.1"
PORT = 50368

def recv_response(sock):
    data = sock.recv(4096)
    if data:
        print("Server response:")
        print(data.decode().strip())
    else:
        print("No response from server")

def test_normal(sock):
    print("\n=== TEST: NORMAL FRAME ===")
    payload = "HELLO"
    message = f"LEN:{len(payload)}\n{payload}"
    print("Sending:", repr(message))
    sock.sendall(message.encode())
    recv_response(sock)

def test_partial_recv(sock):
    print("\n=== TEST: PARTIAL RECV ===")
    part1 = "LEN:5\nHE"
    part2 = "LLO"

    print("Sending part 1:", repr(part1))
    sock.sendall(part1.encode())

    time.sleep(1)

    print("Sending part 2:", repr(part2))
    sock.sendall(part2.encode())

    recv_response(sock)

def test_multiple_messages_one_buffer(sock):
    print("\n=== TEST: MULTIPLE MESSAGES IN ONE BUFFER ===")
    combined = "LEN:5\nHELLOLEN:5\nWORLD"
    print("Sending combined:", repr(combined))
    sock.sendall(combined.encode())

    # Expecting two responses
    recv_response(sock)
    recv_response(sock)

def test_invalid_length(sock):
    print("\n=== TEST: INVALID LENGTH ===")
    bad = "LEN:abc\nHELLO"
    print("Sending:", repr(bad))
    sock.sendall(bad.encode())
    recv_response(sock)

def test_oversized_payload(sock):
    print("\n=== TEST: OVERSIZED PAYLOAD ===")
    payload = "A" * 5000
    bad = f"LEN:{len(payload)}\n{payload}"
    print("Sending oversized payload header only shown")
    print(repr(f"LEN:{len(payload)}\\n<5000 bytes payload>"))
    sock.sendall(bad.encode())
    recv_response(sock)

def main():
    print("Choose test:")
    print("1 - Normal frame")
    print("2 - Partial recv")
    print("3 - Multiple messages in one buffer")
    print("4 - Invalid length")
    print("5 - Oversized payload")

    choice = input("Enter choice: ").strip()

    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.connect((HOST, PORT))

    if choice == "1":
        test_normal(sock)
    elif choice == "2":
        test_partial_recv(sock)
    elif choice == "3":
        test_multiple_messages_one_buffer(sock)
    elif choice == "4":
        test_invalid_length(sock)
    elif choice == "5":
        test_oversized_payload(sock)
    else:
        print("Invalid choice")

    sock.close()

if __name__ == "__main__":
    main()
