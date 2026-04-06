import socket
import threading
import time

HOST = "127.0.0.1"
PORT = 50368
NUM_CLIENTS = 10

def client_task(client_id):
    try:
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.connect((HOST, PORT))

        payload = f"HELLO_FROM_CLIENT_{client_id}"
        message = f"LEN:{len(payload)}\n{payload}"

        print(f"[Client {client_id}] Sending: {repr(message)}")
        sock.sendall(message.encode())

        response = sock.recv(4096)
        print(f"[Client {client_id}] Response: {response.decode().strip()}")

        # Keep connection alive for a few seconds
        time.sleep(5)

        sock.close()
        print(f"[Client {client_id}] Closed")
    except Exception as e:
        print(f"[Client {client_id}] Error: {e}")

threads = []

for i in range(1, NUM_CLIENTS + 1):
    t = threading.Thread(target=client_task, args=(i,))
    t.start()
    threads.append(t)

for t in threads:
    t.join()

print("All clients finished.")
