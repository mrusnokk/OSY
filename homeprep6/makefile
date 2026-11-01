CXX      := g++
CXXFLAGS := -Wall -Wextra -std=c++17

TARGETS := zoo_server zoo_client zoo_info

.PHONY: all clean

all: $(TARGETS)

zoo_server: server.cpp
	$(CXX) $(CXXFLAGS) -o $@ $<

zoo_client: client.cpp
	$(CXX) $(CXXFLAGS) -o $@ $<

zoo_info: zoo_info.cpp
	$(CXX) $(CXXFLAGS) -o $@ $<

clean:
	rm -f $(TARGETS)