import socket
import struct

s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
s.connect(('127.0.0.1', 12345))

username = b'testuser\x00' + b'\x00' * 23 
s.send(struct.pack('>BH', 0x01, 32) + username)

data = s.recv(7)
packet_type, size, client_id = struct.unpack('>BHI', data)
print(f'type: {hex(packet_type)}')
print(f'client_id: {client_id}')

s.send(struct.pack('>BH', 0x0A, 0))
data = s.recv(3)
packet_type, size = struct.unpack('>BH', data)
print(f'type: {hex(packet_type)}')

s.close()