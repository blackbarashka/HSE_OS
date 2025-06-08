CC = g++

all: server client monitor manager

server: bee_server_10.cpp
	$(CC) -o bee_server_10 bee_server_10.cpp

client: bee_client_10.cpp
	$(CC) -o bee_client_10 bee_client_10.cpp

monitor: bee_monitor_10.cpp
	$(CC) -o bee_monitor_10 bee_monitor_10.cpp

manager: bee_manager.cpp
	$(CC) -o bee_manager bee_manager.cpp

clean:
	rm -f bee_server_10 bee_client_10 bee_monitor_10 bee_manager

