CXX := g++

CXXFLAGS := -Iinclude -IStackDead-main -Wshadow -Winit-self -Wredundant-decls -Wcast-align -Wundef -Wfloat-equal -Winline -Wunreachable-code \
-Wmissing-declarations -Wmissing-include-dirs -Wswitch-enum -Wswitch-default -Weffc++ -Wmain -Wextra -Wall -g -pipe \
-fexceptions -Wcast-qual -Wconversion -Wctor-dtor-privacy -Wempty-body -Wformat-security -Wformat=2 -Wignored-qualifiers \
-Wlogical-op -Wno-missing-field-initializers -Wnon-virtual-dtor -Woverloaded-virtual -Wpointer-arith -Wsign-promo \
-Wstack-usage=8192 -Wstrict-aliasing -Wstrict-null-sentinel -Wtype-limits -Wwrite-strings -Werror=vla -D_DEBUG \
-D_EJUDGE_CLIENT_SIDE -DVERIFY_DEBUG 
LDFLAGS :=

SRC_DIR := source
STACK_DIR := libs/StackDead-main
LIST_DIR := libs/List
BUILD_DIR := build
BIN_DIR := bin
TARGET := $(BIN_DIR)/tree_demo

SOURCES := $(wildcard $(SRC_DIR)/*.cpp) $(STACK_DIR)/stack.cpp $(LIST_DIR)/list_operations.cpp $(LIST_DIR)/list_verification.cpp
OBJECTS := $(SOURCES:%.cpp=$(BUILD_DIR)/%.o)

$(TARGET): $(OBJECTS) | $(BIN_DIR)
	@$(CXX) $(OBJECTS) $(LDFLAGS) -o $@

$(BUILD_DIR)/%.o: %.cpp | $(BUILD_DIR)
	@mkdir -p $(dir $@)
	@$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD_DIR):
	mkdir -p $@

$(BIN_DIR):
	mkdir -p $@

.PHONY: clean
clean:
	rm -rf $(BUILD_DIR) $(BIN_DIR)

