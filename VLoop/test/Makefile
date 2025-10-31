# VLoop Unit Tests Makefile

# Compiler settings
CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -g -I../src -I../../references/googletest/googletest/include
LDFLAGS = -L../../references/googletest/build/lib -lgtest -lgtest_main -pthread

# Directories
SRC_DIR = ../src
TEST_DIR = .
BUILD_DIR = build

# Source files
TEST_SOURCES = $(wildcard $(TEST_DIR)/*.cpp)
TEST_OBJECTS = $(TEST_SOURCES:$(TEST_DIR)/%.cpp=$(BUILD_DIR)/%.o)

# Target
TEST_EXECUTABLE = $(BUILD_DIR)/vloop_tests

.PHONY: all clean test setup

all: setup $(TEST_EXECUTABLE)

setup:
	@mkdir -p $(BUILD_DIR)

$(TEST_EXECUTABLE): $(TEST_OBJECTS)
	$(CXX) $(TEST_OBJECTS) -o $@ $(LDFLAGS)

$(BUILD_DIR)/%.o: $(TEST_DIR)/%.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

test: $(TEST_EXECUTABLE)
	./$(TEST_EXECUTABLE)

clean:
	rm -rf $(BUILD_DIR)

# Google Test setup (run once)
setup-gtest:
	@echo "Setting up Google Test..."
	cd ../../references/googletest && mkdir -p build && cd build && cmake .. && make

.PHONY: setup-gtest