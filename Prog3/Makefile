CXX		 := g++
CXXFLAGS := --std=c++17 -Wall -Wextra
TARGETS  := tsamclient tsamserver
SOURCES  := $(shell find Prog3 -name '*.cpp)
OBJECTS	 := $(SOURCE:.cpp=.o)

all: $(TARGETS)

$(TARGETS): $(OBJECTS)
	$(CXX) $(CXXFLAGS) -o $@ $^

%.o : %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(OBJECTS) $(TARGETS)

.SILENT: clean

.PHONY: all clean