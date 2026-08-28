import socket
import traceback

# Testing the server as a client
HOST = "127.0.0.1"
PORT = 6253

# Buffer size
BUFSIZE = 8192

# Create a TCP socket cuz I did that in socket.cpp
with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as cli:
    # Connect to server
    cli.connect((HOST, PORT))
    print(f"Connected to server at {HOST}:{PORT}")
    while True:
        try:
            # Send message
            message = str(input("Input message: "))
            cli.sendall(message.encode('utf-8'))

            received_data = bytearray()
            while True:
                # Receive response
                data = cli.recv(BUFSIZE)
                if data.endswith("\nEND\n".encode('utf-8')):
                    received_data += data[:-5]
                    break
                else:
                    received_data += data

            print(f"Received: {received_data.decode('utf-8')}")
        except Exception as e:
            traceback.print_exc()
            break

    cli.close()