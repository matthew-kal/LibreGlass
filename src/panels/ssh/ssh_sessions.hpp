#pragma once
#include <cstdint>
#include <optional>
#include <sys/types.h>

// Opaque type declared by libsystemd's userspace API
struct sd_login_monitor;

// C++ OOP wrapper on the systemd's C sd-login API

class SshSessions {
public:
    ~SshSessions();
    SshSessions() = default;
    SshSessions(const SshSessions&) = delete;
    SshSessions& operator=(const SshSessions&) = delete;
    bool initialize();

    // Wrappers are marked noexcept because failure cases are 
    // denoted with noexcept because C functions cannot 
    // emit C++ exceptions 

    int fd() const noexcept;
    int events() const noexcept;
    std::uint64_t timeout() const noexcept;
    std::optional<unsigned> refresh() noexcept;
private:
    sd_login_monitor* monitor_{};
    uid_t uid_{};
};
