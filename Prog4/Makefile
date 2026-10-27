CXX      := g++
CXXFLAGS := -std=c++17 -Wall -Wextra -fsanitize=address,undefined -MMD -MP
LDFLAGS  := -fsanitize=address,undefined
OBJ_DIR  := obj
TARGETS  := tsamclient tsamserver

CLIENT_OBJS := $(addprefix $(OBJ_DIR)/, tsamclient.o message_protocol.o logger.o )
SERVER_OBJS := $(addprefix $(OBJ_DIR)/, tsamserver.o message_protocol.o logger.o)
OBJECTS     := $(sort $(CLIENT_OBJS) $(SERVER_OBJS))

all: $(TARGETS)
	@echo All targets built

tsamclient: $(CLIENT_OBJS)
	$(CXX) $(LDFLAGS) -o $@ $^

tsamserver: $(SERVER_OBJS)
	$(CXX) $(LDFLAGS) -o $@ $^

$(OBJ_DIR)/%.o: %.cpp | $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(OBJ_DIR):
	mkdir -p $@

clean:
	rm -rf $(OBJ_DIR) $(TARGETS)

-include $(OBJECTS:.o=.d)

.PHONY: all clean