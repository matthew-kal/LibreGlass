#include "src/drm_device.hpp"
#include "src/panels/ssh/ssh_panel.hpp"
#include "src/renderer.hpp"
#include "src/panels/process/process_panel.hpp"
#include "src/panels/ssh/ssh_sessions.hpp"
#include <algorithm>
#include <cerrno>
#include <csignal>
#include <cstring>
#include <iostream>
#include <memory>
#include <poll.h>
#include <sys/signalfd.h>
#include <time.h>
#include <unistd.h>

int main(int argc, char** argv)
{
    if (argc > 2 || (argc == 2 && std::strcmp(argv[1], "--help") == 0)) {
        std::cout << "Usage: " << argv[0] << " [/dev/dri/cardN]\n";
        return argc > 2 ? 1 : 0;
    }

    // Create a signal set with empty behavior 
    // Add SIGINT (typically user termination) 
    // and SIGTERM (typically programatic termination)
    // Block direct termination of the process via these means to enable 
    // the process to define behavior upon abrupt termination (cleanup)
    // Open a fd to enable our process to recieve signals in a controlled manner. 
    // Create an access type for said fd. 
    sigset_t signals;
    sigemptyset(&signals);
    sigaddset(&signals, SIGINT); sigaddset(&signals, SIGTERM);
    if (sigprocmask(SIG_BLOCK, &signals, nullptr) != 0) return 1;
    const int signalFd = signalfd(-1, &signals, SFD_CLOEXEC | SFD_NONBLOCK);
    if (signalFd < 0) return 1;
    struct SignalOwner{ 
        int fd; 
        ~SignalOwner(){ 
            close(fd); 
        } 
    } signalOwner{signalFd};

    SshSessions sessions;
    if (!sessions.initialize()) {
        std::cerr << "Could not monitor login sessions.\n";
        return 1;
    }

    DrmDevice device{};

    // Renderer owns both panels and outlives the ref 
    // SshPanel is declared as Lvalue for downstream logic
    auto panel = std::make_unique<SshPanel>();
    auto& ssh = *panel; 
    Renderer renderer{std::move(panel), std::make_unique<ProcessPanel>(device)};

    // Determine if user(s) are connected via ssh before display loop begins
    // sessions.refresh() returns std::optional which is why we have pointer 
    // derefrencing syntax on a non-pointer value 
    const auto initial = sessions.refresh();
    if (!initial) { std::cerr << "Could not read SSH sessions.\n"; return 1; }
    ssh.setConnected(*initial != 0, Panel::Clock::now());

    if (!device.initialize(argc == 2 ? argv[1] : "/dev/dri/card1")) return 1;

    bool running = true, failed = false;
    auto nextConnectionCheck = Panel::Clock::now();
    auto flipStarted = Panel::Clock::time_point{};
    std::cout << "Panels are running. Ctrl+C restores the display.\n";

    while (running) {

        const auto now = Panel::Clock::now();
        
        // Terminate for a broken connection, checked every second
        if (now >= nextConnectionCheck) {
            if (!device.connected()) break;
            nextConnectionCheck = now + std::chrono::seconds(1);
        }

        if (device.flipPending() && now - flipStarted > std::chrono::seconds(2)) {
            std::cerr << "Display flip timed out.\n"; failed = true; break;
        }

        renderer.updateDue(now);

        // Flip if the renderer is ready to submit panel work 
        // and the previous display switch has completed 
        if (renderer.pending() && !device.flipPending()) {
            const auto frame = device.acquireFrame();
            if (!frame || !renderer.render(*frame) || !device.present(*frame)) {
                std::cerr << "Could not construct/present frame.\n"; failed = true; 
                break;
            }
            renderer.submitted();
            if (device.flipPending()) flipStarted = now;
        }

        auto deadline = std::min(renderer.nextUpdate(), nextConnectionCheck);
        if (device.flipPending()) deadline = std::min(deadline, flipStarted + std::chrono::seconds(2));
        auto wait = std::max<long long>(0,
            std::chrono::duration_cast<std::chrono::milliseconds>(deadline - Panel::Clock::now()).count());
        // sd-login's timeout uses CLOCK_MONOTONIC, independent of C++ clock epochs.
        const auto loginTimeout = sessions.timeout();
        if (loginTimeout != UINT64_MAX) {
            timespec ts{}; clock_gettime(CLOCK_MONOTONIC, &ts);
            const auto us = std::uint64_t(ts.tv_sec)*1000000 + ts.tv_nsec/1000;
            wait = std::min<long long>(wait, loginTimeout > us ? (loginTimeout-us+999)/1000 : 0);
        }
        pollfd fds[]{{signalFd, POLLIN, 0}, {device.eventFd(), POLLIN, 0},
                     {sessions.fd(), static_cast<short>(sessions.events()), 0}};
        const int result = poll(fds, 3, static_cast<int>(wait));
        if (result < 0) { if (errno == EINTR) continue; failed = true; break; }
        if (fds[0].revents & POLLIN) running = false;
        if (!running) break;
        for (const auto& fd : fds)
            if (fd.revents & (POLLERR | POLLHUP | POLLNVAL)) failed = true;
        if (failed) break;
        if ((fds[1].revents & POLLIN) && !device.processEvents()) { failed = true; break; }
        if (fds[2].revents || loginTimeout != UINT64_MAX) {
            const auto count = sessions.refresh();
            if (!count) { std::cerr << "Could not refresh SSH sessions.\n"; failed = true; break; }
            ssh.setConnected(*count != 0, Panel::Clock::now());
        }
    }
    const bool released = device.shutdown();
    return !failed && released ? 0 : 1;
}
