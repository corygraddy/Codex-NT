# VSeq - 4-channel step sequencer for Disting NT
# Build configuration

PLUGIN_NAME := 1VSeq.o
SRC_DIR := src
BUILD_DIR := build

# Adjust if your API path differs
NT_API_PATH := ../distingNT_API
INCLUDES := -I$(NT_API_PATH)/include -Iinclude

TARGET_ARCH := arm-none-eabi-
CXX := $(TARGET_ARCH)c++
CXXFLAGS := -std=c++11 -mcpu=cortex-m7 -mfpu=fpv5-d16 -mfloat-abi=hard -mthumb -fno-exceptions -Os -Wall -MMD -MP

SOURCES := $(wildcard $(SRC_DIR)/*.cpp)
OBJ := $(patsubst $(SRC_DIR)/%.cpp,$(BUILD_DIR)/%.o,$(SOURCES))
DEPS := $(OBJ:.o=.d)

TARGET := $(BUILD_DIR)/$(PLUGIN_NAME)

.PHONY: all clean check_api

all: check_api $(TARGET)

check_api:
	@if [ ! -d "$(NT_API_PATH)/include/distingnt" ]; then \
		echo "Error: NT_API_PATH seems wrong. Expected headers at $(NT_API_PATH)/include/distingnt"; \
		exit 1; \
	fi

$(TARGET): $(OBJ)
	@mkdir -p $(@D)
	@echo "Linking $(TARGET) ..."
	$(CXX) -r -o $(TARGET) $(OBJ)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.cpp
	@mkdir -p $(@D)
	@echo "Compiling $< ..."
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

clean:
	@echo "Cleaning build directory..."
	rm -f $(BUILD_DIR)/*

-include $(DEPS)

