#include "drm_device.hpp"

#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <unistd.h>
#include <xf86drm.h>

DrmDevice::~DrmDevice()
{
    if (resources_ != nullptr) {
        drmModeFreeResources(resources_);
    }

    if (fd_ >= 0) {
        close(fd_);
    }
}

bool DrmDevice::initialize(const char* path)
{
    if (fd_ >= 0) {
        std::cerr << "DrmDevice initialization has already been attempted.\n";
        return false;
    }

    fd_ = open(path, O_RDWR | O_CLOEXEC);

    if (fd_ < 0) {
        std::cerr << "Could not open " << path << ": "
                  << std::strerror(errno) << '\n';
        return false;
    }

    std::cout << "Opened " << path
              << " under fd " << fd_ << '\n';

    if (!drmIsMaster(fd_) && drmSetMaster(fd_) != 0) {
        std::cerr << "Could not become DRM master: "
                  << std::strerror(errno) << '\n';
        return false;
    }

    resources_ = drmModeGetResources(fd_);

    if (resources_ == nullptr) {
        std::cerr << "Could not get DRM resources: "
                  << std::strerror(errno) << '\n';
        return false;
    }

    return true;
}

std::optional<std::uint32_t> DrmDevice::findConnectedHdmiConnector() const
{
    if (fd_ < 0 || resources_ == nullptr) {
        std::cerr << "DrmDevice is not initialized.\n";
        return std::nullopt;
    }

    for (int i = 0; i < resources_->count_connectors; ++i) {
        const std::uint32_t connector_id = resources_->connectors[i];
        drmModeConnector* connector = drmModeGetConnector(fd_, connector_id);

        if (connector == nullptr) {
            std::cerr << "Could not inspect connector " << connector_id << ": "
                      << std::strerror(errno) << '\n';
            continue;
        }

        const bool is_hdmi =
            connector->connector_type == DRM_MODE_CONNECTOR_HDMIA || 
            connector->connector_type == DRM_MODE_CONNECTOR_HDMIB;
        const bool is_connected =
            connector->connection == DRM_MODE_CONNECTED;
        const std::uint32_t found_id = connector->connector_id;

        drmModeFreeConnector(connector);

        if (is_hdmi && is_connected) {
            return found_id;
        }
    }

    return std::nullopt;
}
