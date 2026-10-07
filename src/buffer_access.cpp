#include "drm_device.hpp"

#include <cerrno>
#include <limits>
#include <sys/mman.h>
#include <xf86drm.h>
#include <drm.h>
#include <drm_mode.h>

DrmDevice::BufferAccess::BufferAccess(const DrmDevice& device) noexcept
    : device_(device)
{
}

DrmDevice::BufferAccess::Dimensions DrmDevice::BufferAccess::dimensions() const noexcept
{
    return {device_.selectedDisplay_.mode.hdisplay, device_.selectedDisplay_.mode.vdisplay};
}

bool DrmDevice::BufferAccess::allocateDumbBuffer(
    std::uint32_t width, std::uint32_t height, std::uint32_t bpp,
    Allocation& allocation) const noexcept
{
    drm_mode_create_dumb request{};
    request.width = width;
    request.height = height;
    request.bpp = bpp;
    if (drmIoctl(device_.fd_.get(), DRM_IOCTL_MODE_CREATE_DUMB, &request) != 0) {
        return false;
    }
    allocation = {request.handle, request.pitch, request.size};
    return true;
}

bool DrmDevice::BufferAccess::registerFramebuffer(
    std::uint32_t width, std::uint32_t height, std::uint32_t format,
    std::uint32_t handle, std::uint32_t pitch, std::uint32_t& id) const noexcept
{
    const std::uint32_t handles[4] = {handle, 0, 0, 0};
    const std::uint32_t pitches[4] = {pitch, 0, 0, 0};
    const std::uint32_t offsets[4] = {};
    return drmModeAddFB2(device_.fd_.get(), width, height, format,
                         handles, pitches, offsets, &id, 0) == 0;
}

bool DrmDevice::BufferAccess::mappingOffset(
    std::uint32_t handle, std::uint64_t& offset) const noexcept
{
    drm_mode_map_dumb request{};
    request.handle = handle;
    if (drmIoctl(device_.fd_.get(), DRM_IOCTL_MODE_MAP_DUMB, &request) != 0) {
        return false;
    }
    offset = request.offset;
    return true;
}

void* DrmDevice::BufferAccess::mapBuffer(std::size_t size, std::uint64_t offset) const noexcept
{
    if (offset > static_cast<std::uint64_t>(std::numeric_limits<off_t>::max())) {
        errno = EOVERFLOW;
        return MAP_FAILED;
    }
    return mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_SHARED,
                device_.fd_.get(), static_cast<off_t>(offset));
}

bool DrmDevice::BufferAccess::removeFramebuffer(std::uint32_t id) const noexcept
{
    return drmModeRmFB(device_.fd_.get(), id) == 0;
}

bool DrmDevice::BufferAccess::destroyDumbBuffer(std::uint32_t handle) const noexcept
{
    drm_mode_destroy_dumb request{};
    request.handle = handle;
    return drmIoctl(device_.fd_.get(), DRM_IOCTL_MODE_DESTROY_DUMB, &request) == 0;
}

bool DrmDevice::BufferAccess::canReleaseBuffer() const noexcept
{
    return !device_.displayActive_;
}
