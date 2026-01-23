# V3Seq Makefile

# Use local ARM toolchain if available, otherwise use system
ARM_TOOLCHAIN := $(HOME)/arm-toolchain
ifneq ($(wildcard $(ARM_TOOLCHAIN)/bin/arm-none-eabi-c++),)
	CXX := $(ARM_TOOLCHAIN)/bin/arm-none-eabi-c++
else
	TARGET_ARCH := arm-none-eabi-
	CXX := $(TARGET_ARCH)c++
endif

CXXFLAGS := -std=c++11 -mcpu=cortex-m7 -mfpu=fpv5-d16 -mfloat-abi=hard -mthumb -fno-exceptions -Os -Wall -MMD -MP
NT_API_PATH := ../distingNT_API/include
INCLUDES := -I$(NT_API_PATH) -Iinclude

SRC_DIR := src
BUILD_DIR := build
TARGET := $(BUILD_DIR)/1V3Seq.o

SOURCES := $(wildcard $(SRC_DIR)/*.cpp)
OBJECTS := $(patsubst $(SRC_DIR)/%.cpp, $(BUILD_DIR)/%.o, $(SOURCES))
DEPS := $(OBJECTS:.o=.d)

.PHONY: all clean check_api

all: check_api $(TARGET)

check_api:
	@if [ ! -d "$(NT_API_PATH)" ]; then \
		echo "ERROR: distingNT API not found at $(NT_API_PATH)"; \
		exit 1; \
	fi

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.cpp | $(BUILD_DIR)
	@echo "Compiling $< ..."
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

$(TARGET): $(OBJECTS)
	@echo "Linking $(TARGET) ..."
	$(CXX) -r -o $(TARGET) $(OBJECTS)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

clean:
	rm -rf $(BUILD_DIR)

-include $(DEPS)
