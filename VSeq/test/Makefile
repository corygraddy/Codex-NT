# VSeq Unit Tests Makefile
# Standalone tests without external dependencies

CXX = clang++
CXXFLAGS = -std=c++11 -Wall
LDFLAGS = 

# Test files
TEST_SRCS = test_vseq.cpp
TEST_OBJS = $(TEST_SRCS:.cpp=.o)

# Output binary
TEST_BIN = vseq_tests

.PHONY: all clean test run

all: $(TEST_BIN)

$(TEST_BIN): $(TEST_OBJS)
	@echo "Linking tests..."
	$(CXX) -o $@ $^ $(LDFLAGS)
	@echo "Built test binary: $(TEST_BIN)"

%.o: %.cpp
	@echo "Compiling $<..."
	$(CXX) $(CXXFLAGS) -c $< -o $@

test: $(TEST_BIN)
	@echo ""
	@echo "Running VSeq unit tests..."
	@echo "================================"
	./$(TEST_BIN)

run: test

clean:
	@echo "Cleaning test build artifacts..."
	rm -f $(TEST_OBJS) $(TEST_BIN)
	@echo "Clean complete."
