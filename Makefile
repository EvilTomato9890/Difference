CXX := g++

CXXFLAGS := -Iinclude -IStackDead-main -Wshadow -Winit-self -Wredundant-decls -Wcast-align -Wundef -Wfloat-equal -Winline -Wunreachable-code \
-Wmissing-declarations -Wmissing-include-dirs -Wswitch-enum -Wswitch-default -Weffc++ -Wmain -Wextra -Wall -g -pipe \
-fexceptions -Wcast-qual -Wconversion -Wctor-dtor-privacy -Wempty-body -Wformat-security -Wformat=2 -Wignored-qualifiers \
-Wlogical-op -Wno-missing-field-initializers -Wnon-virtual-dtor -Woverloaded-virtual -Wpointer-arith -Wsign-promo \
-Wstack-usage=8192 -Wstrict-aliasing -Wstrict-null-sentinel -Wtype-limits -Wwrite-strings -Werror=vla -D_DEBUG -DVERIFY_DEBUG

SRC_DIR := source
BUILD_DIR := build
BIN_DIR := bin
TARGET := $(BIN_DIR)/tree_demo

ALL_SOURCES := $(wildcard $(SRC_DIR)/*.cpp) 
OBJECTS_CONSOLE := $(SOURCES_CONSOLE:%.cpp=$(BUILD_DIR)/%.o)

$(TARGET): $(OBJECTS_CONSOLE) | $(BIN_DIR)
	@$(CXX) $(OBJECTS_CONSOLE) $(LDFLAGS_CONSOLE) -o $@

$(BUILD_DIR)/%.o: %.cpp | $(BUILD_DIR)
	@mkdir -p $(dir $@)
	@$(CXX) $(CXXFLAGS) $(RAYLIB_INCLUDE) -c $< -o $@

$(BUILD_DIR):
	mkdir -p $@

$(BIN_DIR):
	mkdir -p $@

.PHONY: all clean console
all: $(TARGET)

clean:
	rm -rf $(BUILD_DIR) $(BIN_DIR)
