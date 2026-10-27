CXX      := g++
CXXFLAGS := -std=c++17 -Wall -Wextra -fsanitize=address,undefined -MMD -MP
LDFLAGS  := -fsanitize=address,undefined
OBJ_DIR  := obj
TARGETS  := tsamserver

OBJECTS := $(addprefix $(OBJ_DIR)/, tsamserver.o message_protocol.o log_event.o)
OBJECTS := $(addprefix $(OBJ_DIR)/, tsamserver.o message_protocol.o log_event.o)

all: $(TARGETS)
	@echo All targets built

tsamserver: $(OBJECTS)
	$(CXX) $(LDFLAGS) -o $@ $^

$(OBJ_DIR)/%.o: %.cpp | $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(OBJ_DIR):
	mkdir -p $@

clean:
	rm -rf $(OBJ_DIR) $(TARGETS)

-include $(OBJECTS:.o=.d)

.PHONY: all clean