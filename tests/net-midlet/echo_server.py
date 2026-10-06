import socket, threading
s=socket.socket(); s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR,1); s.bind(('127.0.0.1',5555)); s.listen(1)
c,_=s.accept()
d=c.recv(4096); c.sendall(d); c.close()
