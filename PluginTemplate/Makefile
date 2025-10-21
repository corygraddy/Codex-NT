# --- Configuration ---

# The name of your final plugin file.
PLUGIN_NAME := PluginTemplate.o

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
CXXFLAGS := -std=c++11 -mcpu=cortex-m7 -mfpu=fpv5-d16 -mfloat-abi=hard -mthumb -fno-exceptions -Os -fPIC -Wall

# --- Build Rules ---

# Find all .cpp files in the source directory.
SOURCES := $(wildcard $(SRC_DIR)/*.cpp)

# The final target plugin file in the build directory.
TARGET := $(BUILD_DIR)/$(PLUGIN_NAME)

.PHONY: all clean

# Default rule.
all: $(TARGET)

# Rule to build the plugin.
$(TARGET): $(SOURCES)
	@mkdir -p $(@D)
	@echo "Compiling $(SOURCES) into $(TARGET)..."
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $(SOURCES) -o $(TARGET)

# Rule to clean the build directory.
clean:
	@echo "Cleaning build directory..."
	rm -f $(BUILD_DIR)/*