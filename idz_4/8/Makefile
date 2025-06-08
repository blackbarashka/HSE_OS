CC = g++

all: server client monitor

server: bee_server_8.cpp
	$(CC) -o bee_server_8 bee_server_8.cpp

client: bee_client_8.cpp
	$(CC) -o bee_client_8 bee_client_8.cpp

monitor: bee_monitor_8.cpp
	$(CC)  -o bee_monitor_8 bee_monitor_8.cpp

clean:
	rm -f bee_server_8 bee_client_8 bee_monitor_8
