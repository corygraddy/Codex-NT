TARGET = VFader
BUILD_DIR = build
SRC_DIR = src
SRCS = $(SRC_DIR)/main.cpp
OBJS = $(BUILD_DIR)/main.o

CXX = arm-none-eabi-c++
CXXFLAGS = -std=c++11 -mcpu=cortex-m7 -mfpu=fpv5-d16 -mfloat-abi=hard -mthumb -fno-exceptions -Os -Wall -MMD -MP -I../distingNT_API/include -Iinclude
LDFLAGS = -r

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.cpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(BUILD_DIR)/$(TARGET).o: $(OBJS)
	$(CXX) $(LDFLAGS) -o $@ $^

all: $(BUILD_DIR)/$(TARGET).o

clean:
	rm -rf $(BUILD_DIR)

.PHONY: all clean# VFader - I2C→Virtual faders→14-bit MIDI CC pairs

PLUGIN_NAME := VFader.o
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
