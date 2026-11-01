CXX      := g++
CXXFLAGS := -Wall -Wextra -std=c++17 -pthread

TARGETS := zoo_server zoo_client zoo_info 

.PHONY: all clean

all: $(TARGETS)

zoo_server: server.cpp 
	$(CXX) $(CXXFLAGS) -o $@ $< 

zoo_client: client.cpp zoo_map.h libzoomap.so
	$(CXX) $(CXXFLAGS) -o zoo_client client.cpp -L. -lzoomap -Wl,-rpath,.

zoo_info: zoo_info.cpp
	$(CXX) $(CXXFLAGS) -o $@ $<

clean:
	rm -f $(TARGETS)