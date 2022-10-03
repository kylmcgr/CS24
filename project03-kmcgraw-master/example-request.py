import socket

METHOD = b"GET"
URL = b"/"
HEADERS = b"Host: www.example.com"

client = socket.socket(socket.AF_INET, socket.SOCK_STREAM)

# By default, web browsers request unencrypted websites on port 80.
# We want to simulate a "default" request here.
client.connect(("example.com", 80))
request = METHOD + b" " + URL + b" " + b"HTTP/1.1\r\n" + HEADERS + b"\r\nConnection: close\r\n\r\n"
print(request)
client.send(request)
response = client.recv(4096)
print(response.decode())