CXX := g++
CXXFLAGS := --std=c++17 -Wall -Wextra -W -fsanitize=address -fsanitize=undefined
TARGETS := tsamclient tsamserver
OBJECTS := message_protocol.o tsamclient.o tsamserver.o

all: $(TARGETS)

tsamclient: tsamclient.o message_protocol.o
	$(CXX) $(CXXFLAGS) -o $@ $^

tsamserver: tsamserver.o message_protocol.o
	$(CXX) $(CXXFLAGS) -o $@ $^

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(OBJECTS) $(TARGETS)

.PHONY: all clean