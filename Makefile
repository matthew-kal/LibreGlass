CXX := g++
CXXFLAGS := -std=c++20 -Wall -Wextra $(shell pkg-config --cflags libdrm)
LDLIBS := $(shell pkg-config --libs libdrm)

SOURCES := main.cpp src/drm_device.cpp src/drm_buffer.cpp src/buffer_access.cpp
BUILD_DIR ?= build
OBJECTS := $(addprefix $(BUILD_DIR)/,$(SOURCES:.cpp=.o))
TARGET := $(BUILD_DIR)/libre-glass
TEST_TARGET := $(BUILD_DIR)/drm-lifecycle-test
TEST_OBJECTS := $(BUILD_DIR)/tests/drm_lifecycle_test.o $(BUILD_DIR)/src/drm_device.o $(BUILD_DIR)/src/drm_buffer.o $(BUILD_DIR)/src/buffer_access.o
TEST_WRAPS := open close drmIsMaster drmSetMaster drmModeGetResources drmModeFreeResources \
              drmModeGetConnectorCurrent drmModeGetConnector drmModeFreeConnector \
              drmModeGetEncoder drmModeFreeEncoder drmModeGetCrtc drmModeFreeCrtc \
              drmIoctl drmModeAddFB2 drmModeRmFB mmap munmap _Znwm

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $(OBJECTS) $(LDLIBS) -o $@

$(BUILD_DIR)/%.o: %.cpp
	mkdir -p $(@D)
	$(CXX) $(CXXFLAGS) -MMD -MP -c $< -o $@

$(TEST_TARGET): $(TEST_OBJECTS)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $(TEST_OBJECTS) $(foreach symbol,$(TEST_WRAPS),-Wl,--wrap=$(symbol)) $(LDLIBS) -o $@

test: $(TEST_TARGET)
	$(TEST_TARGET)

-include $(OBJECTS:.o=.d) $(TEST_OBJECTS:.o=.d)

clean:
	rm -rf $(BUILD_DIR)

.PHONY: all clean test
