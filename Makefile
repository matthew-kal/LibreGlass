CXX := g++
CXXFLAGS := -std=c++20 -O3 -Wall -Wextra $(shell pkg-config --cflags libdrm)
LDLIBS := $(shell pkg-config --libs libdrm) -l:libsystemd.so.0

SOURCES := main.cpp src/panels/process/process_panel.cpp src/panels/process/kernel_stats.cpp src/panels/ssh/ssh_sessions.cpp src/drm_device.cpp src/drm_buffer.cpp src/buffer_access.cpp src/renderer.cpp src/region_canvas.cpp src/panels/ssh/ssh_panel.cpp src/panels/weather/weather_panel.cpp
BUILD_DIR ?= build
OBJECTS := $(addprefix $(BUILD_DIR)/,$(SOURCES:.cpp=.o))
TARGET := $(BUILD_DIR)/libre-glass
TEST_TARGET := $(BUILD_DIR)/drm-lifecycle-test
TEST_OBJECTS := $(BUILD_DIR)/tests/drm_lifecycle_test.o $(BUILD_DIR)/src/drm_device.o $(BUILD_DIR)/src/drm_buffer.o $(BUILD_DIR)/src/buffer_access.o $(BUILD_DIR)/src/renderer.o $(BUILD_DIR)/src/region_canvas.o
RENDERER_TEST_TARGET := $(BUILD_DIR)/renderer-test
RENDERER_TEST_OBJECTS := $(BUILD_DIR)/src/panels/ssh/ssh_panel.o $(BUILD_DIR)/tests/renderer_test.o $(BUILD_DIR)/src/renderer.o $(BUILD_DIR)/src/region_canvas.o
TEST_WRAPS := open close drmIsMaster drmSetMaster drmModeGetResources drmModeFreeResources \
              drmModeGetConnectorCurrent drmModeGetConnector drmModeFreeConnector \
              drmModeGetEncoder drmModeFreeEncoder drmModeGetCrtc drmModeFreeCrtc \
              drmIoctl drmModeAddFB2 drmModeRmFB drmModeSetCrtc drmModePageFlip drmHandleEvent mmap munmap _Znwm

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $(OBJECTS) $(LDLIBS) -o $@

$(BUILD_DIR)/%.o: %.cpp Makefile
	mkdir -p $(@D)
	$(CXX) $(CXXFLAGS) -MMD -MP -c $< -o $@

$(TEST_TARGET): $(TEST_OBJECTS)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $(TEST_OBJECTS) $(foreach symbol,$(TEST_WRAPS),-Wl,--wrap=$(symbol)) $(LDLIBS) -o $@

$(RENDERER_TEST_TARGET): $(RENDERER_TEST_OBJECTS)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $(RENDERER_TEST_OBJECTS) -o $@

$(BUILD_DIR)/ssh-sessions-test: $(BUILD_DIR)/tests/ssh_sessions_test.o $(BUILD_DIR)/src/panels/ssh/ssh_sessions.o
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $^ -o $@

PROCESS_TEST_OBJECTS := $(BUILD_DIR)/tests/process_panel_test.o $(BUILD_DIR)/src/panels/process/kernel_stats.o $(BUILD_DIR)/src/panels/process/process_panel.o $(BUILD_DIR)/src/renderer.o $(BUILD_DIR)/src/region_canvas.o $(BUILD_DIR)/src/drm_device.o $(BUILD_DIR)/src/drm_buffer.o $(BUILD_DIR)/src/buffer_access.o

$(BUILD_DIR)/process-panel-test: $(PROCESS_TEST_OBJECTS)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $^ $(LDLIBS) -o $@

test: $(BUILD_DIR)/process-panel-test $(TEST_TARGET) $(RENDERER_TEST_TARGET) $(BUILD_DIR)/ssh-sessions-test
	$(TEST_TARGET)
	$(RENDERER_TEST_TARGET)
	$(BUILD_DIR)/ssh-sessions-test
	$(BUILD_DIR)/process-panel-test

-include $(BUILD_DIR)/tests/ssh_sessions_test.d $(BUILD_DIR)/tests/process_panel_test.d

-include $(OBJECTS:.o=.d) $(TEST_OBJECTS:.o=.d) $(RENDERER_TEST_OBJECTS:.o=.d)

clean:
	rm -rf $(BUILD_DIR)

.PHONY: all clean test
