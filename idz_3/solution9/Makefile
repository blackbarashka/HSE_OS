CXX = g++
CXXFLAGS = -std=c++11 -Wall -Wextra
LDFLAGS = -pthread

all: server client observer bee_manager

server: server.cpp
	$(CXX) $(CXXFLAGS) -o server server.cpp $(LDFLAGS)

client: client.cpp
	$(CXX) $(CXXFLAGS) -o client client.cpp $(LDFLAGS)

observer: observer.cpp
	$(CXX) $(CXXFLAGS) -o observer observer.cpp $(LDFLAGS)

bee_manager: bee_manager.cpp
	$(CXX) $(CXXFLAGS) -o bee_manager bee_manager.cpp $(LDFLAGS)

clean:
	rm -f server client observer bee_manager

run-server:
	./server 127.0.0.1 8080

run-client:
	./client 1 127.0.0.1 8080

run-observer:
	./observer 127.0.0.1 8080

run-manager:
	./bee_manager 127.0.0.1 8080

.PHONY: all clean run-server run-client run-observer run-manager
