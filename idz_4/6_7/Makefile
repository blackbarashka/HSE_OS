CC = g++

all: server client monitor

server: server.cpp
	$(CC) -o server server.cpp

client: client.cpp
	$(CC) -o client client.cpp

monitor: monitor.cpp
	$(CC)  -o monitor monitor.cpp

clean:
	rm -f server client monitor
