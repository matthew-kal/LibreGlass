#include "src/drm_device.hpp"
#include "src/panels/ssh/ssh_panel.hpp"
#include "src/renderer.hpp"
#include "src/panels/process/process_panel.hpp"
#include "src/panels/ssh/ssh_sessions.hpp"
#include <algorithm>
#include <cerrno>
#include <csignal>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <memory>
#include <poll.h>
#include <sys/epoll.h>
#include <sys/signalfd.h>
#include <unistd.h>

int main(int argc, char** argv)
{
    if (argc > 2 || (argc == 2 && std::strcmp(argv[1], "--help") == 0)) {
        std::cout << "Usage: " << argv[0] << " [/dev/dri/cardN]\n";
        return argc > 2 ? 1 : 0;
    }

    // Create an initially empty signal set
    // Add SIGINT (typically user termination) 
    // and SIGTERM (typically programatic termination)
    // Block normal delivery of these signals so the event loop can
    // handle termination requests and restore the display before exiting.
    // Open a fd to receive the blocked signals through our event loop.
    // Create an owner that closes the fd.
    sigset_t signals;
    sigemptyset(&signals);
    sigaddset(&signals, SIGINT); sigaddset(&signals, SIGTERM);
    if (sigprocmask(SIG_BLOCK, &signals, nullptr) != 0) return 1;
    const int signalFd = signalfd(-1, &signals, SFD_CLOEXEC | SFD_NONBLOCK);
    if (signalFd < 0) return 1;
    struct FdOwner{ 
        int fd; 
        ~FdOwner(){
            close(fd); 
        } 
    } signalOwner{signalFd};

    SshSessions sessions;
    if (!sessions.initialize()) {
        std::cerr << "Could not monitor login sessions.\n";
        return 1;
    }

    DrmDevice device{};

    // Renderer owns both panels while the reference is used.
    // Keep an SshPanel reference for downstream connection updates.
    auto panel = std::make_unique<SshPanel>();
    auto& ssh = *panel; 
    Renderer renderer{std::move(panel), std::make_unique<ProcessPanel>(device)};

    // Determine if this user has SSH sessions before the display loop begins.
    // sessions.refresh() returns std::optional; its overloaded operator *
    // accesses the contained count after checking for a value.
    const auto initial = sessions.refresh();
    if (!initial) { std::cerr << "Could not read SSH sessions.\n"; return 1; }
    ssh.setConnected(*initial != 0, Panel::Clock::now());

    if (!device.initialize(argc == 2 ? argv[1] : "/dev/dri/card1")) return 1;

    enum class EventSource : std::uint32_t { Signal, Display, Ssh };
    const int epollFd = epoll_create1(EPOLL_CLOEXEC);
    if (epollFd < 0) {
        std::cerr << "Could not create event monitor: " << std::strerror(errno) << '\n';
        return 1;
    }
    FdOwner epollOwner{epollFd};
    const auto watch = [epollFd](int fd, std::uint32_t events, EventSource source) {
        epoll_event event{};
        event.events = events;
        event.data.u32 = static_cast<std::uint32_t>(source);
        if (epoll_ctl(epollFd, EPOLL_CTL_ADD, fd, &event) == 0) return true;
        std::cerr << "Could not register event source: " << std::strerror(errno) << '\n';
        return false;
    };
    const int sessionEvents = sessions.events();
    if (sessionEvents < 0) {
        std::cerr << "Could not query SSH monitor events: " << std::strerror(-sessionEvents) << '\n';
        return 1;
    }
    static_assert(POLLIN == EPOLLIN && POLLPRI == EPOLLPRI && POLLOUT == EPOLLOUT);
    if (!watch(signalFd, EPOLLIN, EventSource::Signal) ||
        !watch(device.eventFd(), EPOLLIN, EventSource::Display) ||
        !watch(sessions.fd(), static_cast<std::uint32_t>(sessionEvents), EventSource::Ssh)) return 1;
    epoll_event events[3]{};

    bool running = true, failed = false;
    auto nextDisplayConnectionCheck = Panel::Clock::now();
    auto flipStarted = Panel::Clock::time_point{};
    std::cout << "Panels are running. Ctrl+C restores the display.\n";

    while (running) {

        const auto now = Panel::Clock::now();
        
        // Check display connection each second; terminate on disconnect or failure.
        if (now >= nextDisplayConnectionCheck) {
            if (!device.connected()) break;
            nextDisplayConnectionCheck = now + std::chrono::seconds(1);
        }

        // Terminate if an async page flip remains pending for over two seconds.
        if (device.flipPending() && now - flipStarted > std::chrono::seconds(2)) {
            std::cerr << "Display flip timed out.\n"; failed = true; break;
        }

        renderer.updateDue(now);

        // Render and submit changed panel state when no display flip is pending.
        // Acquire a drawing buffer, paint it, and present it.
        // Record revisions into submitted and start timing any queued flip.
        if (renderer.pending() && !device.flipPending()) {
            const auto frame = device.acquireFrame();
            if (!frame || !renderer.render(*frame) || !device.present(*frame)) {
                std::cerr << "Could not construct/present frame.\n"; failed = true; 
                break;
            }
            renderer.submitted();
            if (device.flipPending()) flipStarted = now;
        }
        
        auto deadline = std::min(renderer.nextUpdate(), nextDisplayConnectionCheck);
        // Include the pending page flip's two-second completion deadline.
        if (device.flipPending()) deadline = std::min(deadline, flipStarted + std::chrono::seconds(2));
        auto pollWait = std::max<long long>(0,
            std::chrono::duration_cast<std::chrono::milliseconds>(deadline - Panel::Clock::now()).count());
            
        // Wait for termination signals, DRM completion events, or login-session changes.
        const int result = epoll_wait(epollFd, events, 3, static_cast<int>(pollWait));
        if (result < 0) {
            if (errno == EINTR) continue;
            std::cerr << "Could not wait for events: " << std::strerror(errno) << '\n';
            failed = true;
            break;
        }
        bool eventFailed = false;
        for (int i = 0; i < result; ++i) {
            if (events[i].data.u32 == static_cast<std::uint32_t>(EventSource::Signal) &&
                (events[i].events & EPOLLIN)) running = false;
            if (events[i].events & (EPOLLERR | EPOLLHUP)) eventFailed = true;
        }
        if (!running) break;
        if (eventFailed) {
            std::cerr << "An event source reported an error or hangup.\n";
            failed = true;
            break;
        }
        for (int i = 0; i < result && !failed; ++i) {
            switch (static_cast<EventSource>(events[i].data.u32)) {
            case EventSource::Signal:
                break;
            case EventSource::Display:
                if (!device.processEvents()) failed = true;
                break;
            case EventSource::Ssh: {
                const auto count = sessions.refresh();
                if (!count) {
                    std::cerr << "Could not refresh SSH sessions.\n";
                    failed = true;
                    break;
                }
                ssh.setConnected(*count != 0, Panel::Clock::now());
                break;
            }
            }
        }
        if (failed) break;
    }
    const bool released = device.shutdown();
    return !failed && released ? 0 : 1;
}
