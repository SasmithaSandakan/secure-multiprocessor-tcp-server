import socket

HOST = "127.0.0.1"
PORT = 50368

def send_frame(sock, payload):
    message = f"LEN:{len(payload)}\n{payload}"
    print(f"\nSending: {payload}")
    sock.sendall(message.encode())

    response = sock.recv(4096)
    print("Response:")
    print(response.decode().strip())

def main():
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.connect((HOST, PORT))

    while True:
        cmd = input("\nEnter command (REGISTER/LOGIN/WHOAMI/LOGOUT/EXIT): ").strip()

        if cmd.upper() == "EXIT":
            break

        send_frame(sock, cmd)

    sock.close()

if __name__ == "__main__":
    main()
