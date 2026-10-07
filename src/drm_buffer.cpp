#include "drm_buffer.hpp"
#include "drm_device.hpp"

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <limits>
#include <sys/mman.h>
#include <drm_fourcc.h>

namespace {
void reportSystemError(const char* operation, int error) noexcept
{
    std::fprintf(stderr, "%s: %s\n", operation, std::strerror(error));
}
}

DrmBuffer::DrmBuffer(const DrmDevice& device) noexcept
    : device_(device)
{
}

DrmBuffer::~DrmBuffer() noexcept
{
    destroyBuffer();
}

bool DrmBuffer::createBuffer()
{
    if (buffer_.hasResources()) {
        std::fprintf(stderr, "Cannot recreate an allocated DRM buffer.\n");
        return false;
    }

    const auto& access = device_.bufferAccess();
    const auto dimensions = access.dimensions();
    if (dimensions.width == 0 || dimensions.height == 0) {
        std::fprintf(stderr, "A DRM buffer requires nonzero dimensions.\n");
        return false;
    }

    DrmDevice::BufferAccess::Allocation allocation{};
    if (!access.allocateDumbBuffer(dimensions.width, dimensions.height, 32, allocation)) {
        reportSystemError("Could not create dumb buffer", errno);
        return false;
    }

    // Own each resource as soon as it is acquired. Failed rollback retains
    // unresolved ownership in this buffer, allowing device shutdown to retry.
    buffer_.handle = allocation.handle;
    buffer_.pitch = allocation.pitch;
    if (allocation.size > std::numeric_limits<std::size_t>::max()) {
        reportSystemError("Dumb buffer size cannot be represented", EOVERFLOW);
        destroyBuffer();
        return false;
    }
    buffer_.size = static_cast<std::size_t>(allocation.size);
    if (std::uint64_t{buffer_.pitch} < std::uint64_t{dimensions.width} * 4 ||
        allocation.size < std::uint64_t{buffer_.pitch} * dimensions.height) {
        reportSystemError("Dumb buffer layout is too small", EINVAL);
        destroyBuffer();
        return false;
    }

    if (!access.registerFramebuffer(dimensions.width, dimensions.height,
                                     DRM_FORMAT_XRGB8888, buffer_.handle,
                                     buffer_.pitch, buffer_.framebufferId)) {
        reportSystemError("Could not register framebuffer", errno);
        destroyBuffer();
        return false;
    }

    std::uint64_t offset{};
    if (!access.mappingOffset(buffer_.handle, offset)) {
        reportSystemError("Could not get dumb buffer mapping offset", errno);
        destroyBuffer();
        return false;
    }

    void* pixels = access.mapBuffer(buffer_.size, offset);
    if (pixels == MAP_FAILED) {
        reportSystemError("Could not map dumb buffer", errno);
        destroyBuffer();
        return false;
    }

    buffer_.pixels = static_cast<std::uint8_t*>(pixels);
    // mmap can legally return address zero; mapping ownership is independent
    // of whether the address happens to compare equal to nullptr.
    buffer_.mapped = true;
    return true;
}

bool DrmBuffer::destroyBuffer() noexcept
{
    const auto& access = device_.bufferAccess();
    if (buffer_.hasResources() && !access.canReleaseBuffer()) {
        reportSystemError("Cannot release a buffer while the display is active", EBUSY);
        return false;
    }

    bool released = true;
    if (buffer_.mapped) {
        if (munmap(buffer_.pixels, buffer_.size) == 0) {
            buffer_.pixels = nullptr;
            buffer_.mapped = false;
        } else {
            reportSystemError("Could not unmap dumb buffer", errno);
            released = false;
        }
    }

    if (buffer_.framebufferId != 0) {
        if (access.removeFramebuffer(buffer_.framebufferId)) {
            buffer_.framebufferId = 0;
        } else {
            reportSystemError("Could not remove framebuffer", errno);
            released = false;
        }
    }

    // A failed framebuffer removal keeps its backing handle owned for retry.
    if (buffer_.framebufferId == 0 && buffer_.handle != 0) {
        if (access.destroyDumbBuffer(buffer_.handle)) {
            buffer_.handle = 0;
        } else {
            reportSystemError("Could not destroy dumb buffer", errno);
            released = false;
        }
    }

    if (!buffer_.hasResources()) {
        buffer_ = {};
    }
    return released;
}
