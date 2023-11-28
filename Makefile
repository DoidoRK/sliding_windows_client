CXX = g++
CXXFLAGS = -std=c++11 -Wall

SRC_DIR = src
OBJ_DIR = obj
BIN_DIR = bin

EXECUTABLE = wordle_client

SOURCES = $(wildcard $(SRC_DIR)/*.cc)
OBJECTS = $(patsubst $(SRC_DIR)/%.cc,$(OBJ_DIR)/%.o,$(SOURCES))

.PHONY: compile clean run

compile: $(BIN_DIR)/$(EXECUTABLE)

$(BIN_DIR)/$(EXECUTABLE): $(OBJECTS) | $(BIN_DIR)
	$(CXX) $(CXXFLAGS) -o $@ $(OBJECTS)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cc | $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) -c -o $@ $<

$(BIN_DIR) $(OBJ_DIR):
	mkdir -p $@

clean:
	rm -rf $(BIN_DIR) $(OBJ_DIR)

run: $(BIN_DIR)/$(EXECUTABLE)
	./$(BIN_DIR)/$(EXECUTABLE)
