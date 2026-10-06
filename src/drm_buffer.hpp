#pragma once

#include <cstddef>
#include <cstdint>

class DrmDevice;

class DrmBuffer {
public:
    ~DrmBuffer() noexcept;

    DrmBuffer(const DrmBuffer&) = delete;
    DrmBuffer& operator=(const DrmBuffer&) = delete;

    DrmBuffer(DrmBuffer&&) = delete;
    DrmBuffer& operator=(DrmBuffer&&) = delete;

private:
    friend class DrmDevice;

    // Only the containing device can construct or recreate a buffer. The
    // device is nonmovable and outlives its buffers, so this borrow stays valid.
    // Only its BufferAccess interface is used; no display topology or fd is read.
    explicit DrmBuffer(const DrmDevice& device) noexcept;
    bool createBuffer();
    bool destroyBuffer() noexcept;

    struct DumbBuffer {
        std::uint32_t handle{};
        std::uint32_t framebufferId{};
        std::uint32_t pitch{};
        std::size_t size{};
        std::uint8_t* pixels = nullptr;
        bool mapped{false};

        bool hasResources() const noexcept {
            return handle != 0 || framebufferId != 0 || mapped;
        }
    };

    const DrmDevice& device_;
    DumbBuffer buffer_{};
};
