#include "ssh_sessions.hpp"
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <unistd.h>


extern "C" {
    // Create a monitor for login changes in the requested category.
    // Release the monitor and close its file descriptor.
    // Get the monitor's file descriptor for the event loop.
    // Get the events the event loop should watch for.
    // Get the monitor's next wakeup time, if any.
    // Clear a pending monitor wakeup after handling it.
    // Get the login session IDs for a user.
    // Get the PAM service that registered a session, such as sshd.
    // Get a session's state, such as active or closing.
    int sd_login_monitor_new(const char*, sd_login_monitor**);
    sd_login_monitor* sd_login_monitor_unref(sd_login_monitor*);
    int sd_login_monitor_get_fd(sd_login_monitor*);
    int sd_login_monitor_get_events(sd_login_monitor*);
    int sd_login_monitor_get_timeout(sd_login_monitor*, std::uint64_t*);
    int sd_login_monitor_flush(sd_login_monitor*);
    int sd_uid_get_sessions(uid_t, int, char***);
    int sd_session_get_service(const char*, char**);
    int sd_session_get_state(const char*, char**);
}

SshSessions::~SshSessions() { sd_login_monitor_unref(monitor_); }
bool SshSessions::initialize()
{
    uid_ = getuid();
    return !monitor_ && sd_login_monitor_new("session", &monitor_) >= 0;
}

int SshSessions::fd() const noexcept { return sd_login_monitor_get_fd(monitor_); }
int SshSessions::events() const noexcept { return sd_login_monitor_get_events(monitor_); }

std::uint64_t SshSessions::timeout() const noexcept
{
    std::uint64_t value = UINT64_MAX;
    sd_login_monitor_get_timeout(monitor_, &value);
    return value;
}

std::optional<unsigned> SshSessions::refresh() noexcept
{
    if (sd_login_monitor_flush(monitor_) < 0) return std::nullopt;
    char** sessions = nullptr;
    const int n = sd_uid_get_sessions(uid_, 0, &sessions);
    if (n < 0) return std::nullopt;
    unsigned count = 0;
    bool valid = true;
    for (int i = 0; i < n; ++i) {
        char* service = nullptr;
        char* status = nullptr;
        const int a = sd_session_get_service(sessions[i], &service);
        const int b = sd_session_get_state(sessions[i], &status);
        if (a >= 0 && b >= 0) {
            if (std::strcmp(service, "sshd") == 0 && std::strcmp(status, "closing") != 0)
                ++count;
        } else if ((a < 0 && a != -ENOENT && a != -ENXIO) ||
                   (b < 0 && b != -ENOENT && b != -ENXIO)) valid = false;
        std::free(service); std::free(status); std::free(sessions[i]);
    }
    std::free(sessions);
    return valid ? std::optional<unsigned>{count} : std::nullopt;
}
