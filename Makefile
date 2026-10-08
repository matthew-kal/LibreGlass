CXX := g++
CXXFLAGS := -std=c++20 -O2 -Wall -Wextra $(shell pkg-config --cflags libdrm)
LDLIBS := $(shell pkg-config --libs libdrm)

SOURCES := main.cpp src/drm_device.cpp src/drm_buffer.cpp src/buffer_access.cpp src/renderer.cpp src/region_canvas.cpp src/panels/ssh_panel.cpp src/panels/weather_panel.cpp
BUILD_DIR ?= build
OBJECTS := $(addprefix $(BUILD_DIR)/,$(SOURCES:.cpp=.o))
TARGET := $(BUILD_DIR)/libre-glass
TEST_TARGET := $(BUILD_DIR)/drm-lifecycle-test
TEST_OBJECTS := $(BUILD_DIR)/tests/drm_lifecycle_test.o $(BUILD_DIR)/src/drm_device.o $(BUILD_DIR)/src/drm_buffer.o $(BUILD_DIR)/src/buffer_access.o $(BUILD_DIR)/src/renderer.o $(BUILD_DIR)/src/region_canvas.o
RENDERER_TEST_TARGET := $(BUILD_DIR)/renderer-test
RENDERER_TEST_OBJECTS := $(BUILD_DIR)/tests/renderer_test.o $(BUILD_DIR)/src/renderer.o $(BUILD_DIR)/src/region_canvas.o
TEST_WRAPS := open close drmIsMaster drmSetMaster drmModeGetResources drmModeFreeResources \
              drmModeGetConnectorCurrent drmModeGetConnector drmModeFreeConnector \
              drmModeGetEncoder drmModeFreeEncoder drmModeGetCrtc drmModeFreeCrtc \
              drmIoctl drmModeAddFB2 drmModeRmFB drmModeSetCrtc mmap munmap _Znwm

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $(OBJECTS) $(LDLIBS) -o $@

$(BUILD_DIR)/%.o: %.cpp
	mkdir -p $(@D)
	$(CXX) $(CXXFLAGS) -MMD -MP -c $< -o $@

$(TEST_TARGET): $(TEST_OBJECTS)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $(TEST_OBJECTS) $(foreach symbol,$(TEST_WRAPS),-Wl,--wrap=$(symbol)) $(LDLIBS) -o $@

$(RENDERER_TEST_TARGET): $(RENDERER_TEST_OBJECTS)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $(RENDERER_TEST_OBJECTS) -Wl,--wrap=_Znwm -Wl,--wrap=_Znam -o $@

test: $(TEST_TARGET) $(RENDERER_TEST_TARGET)
	$(TEST_TARGET)
	$(RENDERER_TEST_TARGET)

-include $(OBJECTS:.o=.d) $(TEST_OBJECTS:.o=.d) $(RENDERER_TEST_OBJECTS:.o=.d)

clean:
	rm -rf $(BUILD_DIR)

.PHONY: all clean test
