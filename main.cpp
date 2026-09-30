#include "drm_device.hpp"

#include <iostream>

int main()
{
    DrmDevice device{};

    if (!device.initialize("/dev/dri/card1")) {
        return 1;
    }

    const auto hdmi_connector = device.findConnectedHdmiConnector();
    if (!hdmi_connector) {
        std::cerr << "No connected HDMI connector found on this DRM device.\n";
        return 1;
    }

    std::cout << "Selected connected HDMI connector ID "
              << *hdmi_connector << ".\n";

    return 0;
}
