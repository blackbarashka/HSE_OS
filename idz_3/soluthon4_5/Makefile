CXX = g++
CXXFLAGS = -std=c++11 -Wall -Wextra
LDFLAGS = -pthread

all: server client

server: server.cpp
	$(CXX) $(CXXFLAGS) -o server server.cpp $(LDFLAGS)

client: client.cpp
	$(CXX) $(CXXFLAGS) -o client client.cpp $(LDFLAGS)

clean:
	rm -f server client

run-server:
	./server 127.0.0.1 8080

run-client:
	./client 1 127.0.0.1 8080

.PHONY: all clean run-server run-client
