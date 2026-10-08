#include "src/drm_device.hpp"
#include "src/panels/ssh_panel.hpp"
#include "src/panels/weather_panel.hpp"
#include "src/renderer.hpp"

#include <cerrno>
#include <csignal>
#include <cstring>
#include <iostream>
#include <memory>

int main(int argc, char** argv)
{
    if (argc > 2 || (argc == 2 && std::strcmp(argv[1], "--help") == 0)) {
        std::cout << "Usage: " << argv[0] << " [/dev/dri/cardN]\n";
        return argc > 2 ? 1 : 0;
    }

    // Create an empty set of signals the process blocks
    // Block SIGINT ( user termination)
    // Block SIGTERM (typically programatic termination)
    // Attempt to register the set with the kernel
    sigset_t signals;
    sigemptyset(&signals);
    sigaddset(&signals, SIGINT);
    sigaddset(&signals, SIGTERM);
    if (sigprocmask(SIG_BLOCK, &signals, nullptr) != 0) {
        std::cerr << "Could not block termination signals: "
                  << std::strerror(errno) << '\n';
        return 1;
    }

    DrmDevice device{};
    Renderer renderer{
        std::make_unique<SshPanel>(),
        std::make_unique<WeatherPanel>()
    };

    if (!device.initialize(argc == 2 ? argv[1] : "/dev/dri/card1")) {
        return 1;
    }

    const auto now = Panel::Clock::now();
    renderer.update<SshPanel::index>(now);
    renderer.update<WeatherPanel::index>(now);
    const auto frame = device.acquireFrame();
    if (!frame || !renderer.paint(*frame) ||
        !device.present(*frame)) {
        std::cerr << "Could not paint the initial screen.\n";
        return 1;
    }

    std::cout << "Displaying a portrait layout with SSH and weather panel slots. "
              << "Press Ctrl+C to restore the display.\n"
              << std::flush;


    // Attempts to block the main thread of execution
    // Current placeholder for event loop
    int sigwaitRet{};
    const int waitError = sigwait(&signals, &sigwaitRet);

    if (waitError != 0) {
        std::cerr << "Could not wait for termination: "
                  << std::strerror(waitError) << '\n';
    }

    const bool released = device.shutdown();
    return waitError == 0 && released ? 0 : 1;
}
