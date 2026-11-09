CXX := g++
CXXFLAGS := -std=c++17 -Wall -Wextra -Wpedantic -g -pthread

.PHONY: all clean

all: client server

client: client.o
	$(CXX) $(CXXFLAGS) -o client client.o

server: server.o
	$(CXX) $(CXXFLAGS) -o server server.o

client.o: client.cpp utilities.h
	$(CXX) $(CXXFLAGS) -c client.cpp -o client.o

server.o: server.cpp utilities.h serverDataStruct.h
	$(CXX) $(CXXFLAGS) -c server.cpp -o server.o

clean:
	rm -f client server client.o server.o