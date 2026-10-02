#include "drm_device.hpp"

#include <iostream>

int main()
{
    DrmDevice device{};

    if (!device.initialize("/dev/dri/card1")) {
        return 1;
    }

    const auto& display = device.selectedDisplay();
    std::cout << "Selected connector " << display.connectorId
              << ", CRTC " << display.crtcId
              << ", mode " << display.mode.hdisplay << 'x'
              << display.mode.vdisplay << " (" << display.mode.name << ").\n";

    return 0;
}
