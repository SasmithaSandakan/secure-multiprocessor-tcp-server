import socket
import time

HOST = "127.0.0.1"
PORT = 50368

def send_frame(sock, payload):
    message = f"LEN:{len(payload)}\n{payload}"
    sock.sendall(message.encode())
    response = sock.recv(4096)
    print(f"Sent: {payload}")
    print("Response:", response.decode().strip())
    print("-" * 50)

def main():
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.connect((HOST, PORT))

    # Send many requests very quickly on the SAME connection
    for i in range(1, 8):
        send_frame(sock, "WHOAMI")
        time.sleep(0.3)   # small delay, still inside rate window

    sock.close()

if __name__ == "__main__":
    main()
