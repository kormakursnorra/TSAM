CXX = g++
CXXFLAGS = -Wall -Wextra -std=c++11
TARGET = scanner
SOURCES = scanner.cpp
OBJECTS = $(SOURCES:.cpp=.o)

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CXX) $(CXXFLAGS) -o $@ $^

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(OBJECTS) $(TARGET)

.SILENT: clean
