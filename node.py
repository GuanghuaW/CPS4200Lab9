import socket
import sys
import threading

name = sys.argv[1]
addr = sys.argv[2]
port = int(sys.argv[3])

peers = {}

def receiver(server):
    while True:
        conn, _ = server.accept()
        data = conn.recv(1024).decode()
        if not data:
            conn.close()
            continue
        print(f"[{name}] received: {data}")
        if data.startswith("PING"):
            sender = data.split()[1]
            conn.sendall(f"PONG from {name} ({addr}) to {sender}".encode())
        conn.close()

server = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
server.bind(("127.0.0.1", port))
server.listen(5)

print(f"{name} started with logical IP {addr}, listening on port {port}")

threading.Thread(target=receiver, args=(server,), daemon=True).start()

while True:
    cmd = input(f"{name}> ").strip()
    if cmd == "exit":
        break
    elif cmd.startswith("ping"):
        _, target_name, target_addr, target_port = cmd.split()
        s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        s.connect(("127.0.0.1", int(target_port)))
        s.sendall(f"PING {name} {addr}".encode())
        reply = s.recv(1024).decode()
        print(f"[{name}] reply: {reply}")
        s.close()
    else:
        print("Usage: ping <target_name> <target_addr> <target_port>  or exit")
