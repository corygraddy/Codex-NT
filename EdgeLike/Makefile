# --- Configuration ---

# The name of your final plugin file.
PLUGIN_NAME := EdgeLike.o

# The directory where source files are located.
SRC_DIR := src

# The directory where the final plugin will be placed.
BUILD_DIR := build

# --- Paths ---

# Path to the distingNT_API directory.
# Assumes it's in the parent directory, alongside this project folder.
NT_API_PATH := ../distingNT_API

# Include paths for the compiler.
INCLUDES := -I$(NT_API_PATH)/include -Iinclude

# --- Toolchain ---

# ARM GCC toolchain prefix.
TARGET_ARCH := arm-none-eabi-

# Compiler and flags.
CXX := $(TARGET_ARCH)c++
CXXFLAGS := -std=c++11 -mcpu=cortex-m7 -mfpu=fpv5-d16 -mfloat-abi=hard -mthumb -fno-exceptions -Os -Wall -MMD -MP

# --- Build Rules ---

SOURCES := $(wildcard $(SRC_DIR)/*.cpp)
OBJ := $(patsubst $(SRC_DIR)/%.cpp,$(BUILD_DIR)/%.o,$(SOURCES))
DEPS := $(OBJ:.o=.d)

TARGET := $(BUILD_DIR)/$(PLUGIN_NAME)

.PHONY: all clean

# Default rule.
all: check_api $(TARGET)

check_api:
	@if [ ! -d "$(NT_API_PATH)/include/distingnt" ]; then \
		echo "Error: NT_API_PATH seems wrong. Expected headers at $(NT_API_PATH)/include/distingnt"; \
		exit 1; \
	fi

# Rule to build the plugin.
$(TARGET): $(OBJ)
	@mkdir -p $(@D)
	@echo "Linking $(TARGET) ..."
	$(CXX) -r -o $(TARGET) $(OBJ)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.cpp
	@mkdir -p $(@D)
	@echo "Compiling $< ..."
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

# Rule to clean the build directory.
clean:
	@echo "Cleaning build directory..."
	rm -f $(BUILD_DIR)/*

-include $(DEPS)
