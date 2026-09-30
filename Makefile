CXX := g++
CXXFLAGS := -std=c++20 -Wall -Wextra $(shell pkg-config --cflags libdrm)
LDLIBS := $(shell pkg-config --libs libdrm)

OBJECTS := build/main.o build/drm_device.o
TARGET := build/libre-glass

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CXX) $(OBJECTS) $(LDLIBS) -o $@

build/%.o: %.cpp | build
	$(CXX) $(CXXFLAGS) -MMD -MP -c $< -o $@

build:
	mkdir -p build

-include $(OBJECTS:.o=.d)

clean:
	rm -rf build

.PHONY: all clean