import socket
import time

HOST = "127.0.0.1"
PORT = 50368

client = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
client.connect((HOST, PORT))

payload = "HELLO"
message = f"LEN:{len(payload)}\n{payload}"

print("Sending:", repr(message))
client.sendall(message.encode())

response = client.recv(4096)
print("Server response:")
print(response.decode().strip())

print("Keeping connection open for 10 seconds...")
time.sleep(10)

client.close()
print("Client closed.")
