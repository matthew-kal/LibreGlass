#include "src/drm_device.hpp"
#include "src/renderer.hpp"

#include <cerrno>
#include <csignal>
#include <cstring>
#include <iostream>

int main(int argc, char** argv)
{
    if (argc > 2 || (argc == 2 && std::strcmp(argv[1], "--help") == 0)) {
        std::cout << "Usage: " << argv[0] << " [/dev/dri/cardN]\n";
        return argc > 2 ? 1 : 0;
    }

    sigset_t signals{};
    sigemptyset(&signals);
    sigaddset(&signals, SIGINT);
    sigaddset(&signals, SIGTERM);
    if (sigprocmask(SIG_BLOCK, &signals, nullptr) != 0) {
        std::cerr << "Could not block termination signals: "
                  << std::strerror(errno) << '\n';
        return 1;
    }

    DrmDevice device{};
    Renderer renderer{};

    if (!device.initialize(argc == 2 ? argv[1] : "/dev/dri/card1")) {
        return 1;
    }

    const auto& display = device.selectedDisplay();
    std::cout << "Selected connector " << display.connectorId
              << ", CRTC " << display.crtcId
              << ", mode " << display.mode.hdisplay << 'x'
              << display.mode.vdisplay << " (" << display.mode.name << ").\n";

    renderer.update(Panel::Clock::now());
    const auto frame = device.acquireFrame();
    if (!frame || !renderer.paint(*frame, DisplayRotation::Clockwise90) ||
        !device.present(*frame)) {
        std::cerr << "Could not paint the initial screen.\n";
        return 1;
    }

    std::cout << "Displaying an empty portrait layout with nine panel slots. "
              << "Press Ctrl+C to restore the display.\n"
              << std::flush;

    int signal{};
    const int waitError = sigwait(&signals, &signal);
    if (waitError != 0) {
        std::cerr << "Could not wait for termination: "
                  << std::strerror(waitError) << '\n';
    }

    const bool released = device.shutdown();
    return waitError == 0 && released ? 0 : 1;
}
