# Makefile for VCA Plugin

# Project name
PROJECT_NAME := VCA

# Directories
SRC_DIR := src
BUILD_DIR := build

# Include paths
NT_API_PATH := ../distingNT_API
INCLUDES := -I$(NT_API_PATH)/include

# Use local ARM toolchain if available, otherwise use system
ARM_TOOLCHAIN := $(HOME)/arm-toolchain
ifneq ($(wildcard $(ARM_TOOLCHAIN)/bin/arm-none-eabi-c++),)
	CXX := $(ARM_TOOLCHAIN)/bin/arm-none-eabi-c++
else
	TARGET_ARCH := arm-none-eabi-
	CXX := $(TARGET_ARCH)c++
endif

# Compiler flags
CXXFLAGS := -std=c++11 -mcpu=cortex-m7 -mfpu=fpv5-d16 -mfloat-abi=hard \
            -mthumb -fno-exceptions -Os -Wall -MMD -MP

# Source files
SOURCES := $(wildcard $(SRC_DIR)/*.cpp)
OBJECTS := $(patsubst $(SRC_DIR)/%.cpp,$(BUILD_DIR)/%.o,$(SOURCES))
DEPS := $(OBJECTS:.o=.d)

# Output file
OUTPUT := $(BUILD_DIR)/$(PROJECT_NAME).o

# --- Build Rules ---

.PHONY: all clean check_api

# Default target
all: check_api $(OUTPUT)

# Check if the NT_API_PATH is correct
check_api:
	@if [ ! -d "$(NT_API_PATH)/include/distingnt" ]; then \
		echo "Error: distingNT_API not found at $(NT_API_PATH)"; \
		echo "Please set NT_API_PATH in the Makefile to the correct location."; \
		exit 1; \
	fi

# Create build directory
$(BUILD_DIR):
	@mkdir -p $(BUILD_DIR)

# Compile object files
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.cpp | $(BUILD_DIR)
	@echo "Compiling $<..."
	@$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

# Link final output
$(OUTPUT): $(OBJECTS)
	@echo "Linking $(PROJECT_NAME)..."
	@$(CXX) -r $(OBJECTS) -o $@
	@echo "Build complete: $@"

# Include dependencies
-include $(DEPS)

# Clean up build artifacts
clean:
	@echo "Cleaning up..."
	@rm -rf $(BUILD_DIR)
	@echo "Clean complete."
