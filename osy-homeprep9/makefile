	
CXX      := g++
CXXFLAGS := -Wall -Wextra -std=c++17 -pthread
TARGETS := server client

.PHONY: all clean

all: $(TARGETS)

server: server.cpp 
	$(CXX) $(CXXFLAGS) server.cpp -o server 

client: client.cpp
	$(CXX) $(CXXFLAGS) client.cpp -o client

clean:
	rm -f $(TARGETS)