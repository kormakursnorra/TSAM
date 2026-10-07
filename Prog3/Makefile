CXX		 := g++
CXXFLAGS := --std=c++11 -Wall -Wextra
TARGET 	 := tsamserver
SOURCES  := $(shell find Prog3 -name '*.cpp)
OBJECTS	 := $(SOURCE:.cpp=.o)

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CXX) $(CXXFLAGS) -o $@ $^

%.o : %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(OBJECTS) $(TARGET)

.SILENT: clean

.PHONY: all clean