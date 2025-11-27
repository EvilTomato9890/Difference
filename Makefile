CXX := g++

STACK_DIR := libs/StackDead-main
LIST_DIR := libs/List

CXXFLAGS := -Iinclude -I$(STACK_DIR) -I$(LIST_DIR)/include -Wshadow -Winit-self -Wredundant-decls -Wcast-align -Wundef -Wfloat-equal -Winline -Wunreachable-code \
-Wmissing-declarations -Wmissing-include-dirs -Wswitch-enum -Wswitch-default -Weffc++ -Wmain -Wextra -Wall -g -pipe \
-fexceptions -Wcast-qual -Wconversion -Wctor-dtor-privacy -Wempty-body -Wformat-security -Wformat=2 -Wignored-qualifiers \
-Wlogical-op -Wno-missing-field-initializers -Wnon-virtual-dtor -Woverloaded-virtual -Wpointer-arith -Wsign-promo \
-Wstack-usage=8192 -Wstrict-aliasing -Wstrict-null-sentinel -Wtype-limits -Wwrite-strings -Werror=vla -D_DEBUG \
-D_EJUDGE_CLIENT_SIDE -DVERIFY_DEBUG #-DCREATION_DEBUG
LDFLAGS :=

SRC_DIR := source

BUILD_DIR := build
BIN_DIR := bin
TARGET := $(BIN_DIR)/tree_demo

SOURCES := $(wildcard $(SRC_DIR)/*.cpp) $(STACK_DIR)/stack.cpp $(LIST_DIR)/source/list_operations.cpp $(LIST_DIR)/source/list_verification.cpp
OBJECTS := $(patsubst $(SRC_DIR)/%.cpp,$(BUILD_DIR)/$(SRC_DIR)/%.o,$(wildcard $(SRC_DIR)/*.cpp)) \
           $(patsubst $(STACK_DIR)/%.cpp,$(BUILD_DIR)/$(STACK_DIR)/%.o,$(STACK_DIR)/stack.cpp) \
           $(patsubst $(LIST_DIR)/source/%.cpp,$(BUILD_DIR)/$(LIST_DIR)/source/%.o,$(LIST_DIR)/source/list_operations.cpp $(LIST_DIR)/source/list_verification.cpp)

$(TARGET): $(OBJECTS) | $(BIN_DIR)
	@$(CXX) $(OBJECTS) $(LDFLAGS) -o $@

$(BUILD_DIR)/$(SRC_DIR)/%.o: $(SRC_DIR)/%.cpp | $(BUILD_DIR)
	@mkdir -p $(dir $@)
	@$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD_DIR)/$(STACK_DIR)/%.o: $(STACK_DIR)/%.cpp | $(BUILD_DIR)
	@mkdir -p $(dir $@)
	@$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD_DIR)/$(LIST_DIR)/source/%.o: $(LIST_DIR)/source/%.cpp | $(BUILD_DIR)
	@mkdir -p $(dir $@)
	@$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD_DIR):
	mkdir -p $@

$(BIN_DIR):
	mkdir -p $@

.PHONY: clean
clean:
	rm -rf $(BUILD_DIR) $(BIN_DIR)

