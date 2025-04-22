CC = g++
CFLAGS = -Wall -std=c++11 -pthread
TARGET = winnie_search
SRC = main.cpp

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $@ $< -lrt

clean:
	rm -f $(TARGET)
