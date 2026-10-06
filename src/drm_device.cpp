#include "drm_device.hpp"
#include <algorithm>
#include <cassert>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <limits>
#include <memory>
#include <sys/mman.h>
#include <utility>
#include <vector>
#include <unistd.h>
#include <xf86drm.h>
#include <drm.h>
#include <drm_mode.h>

using DRMTypes::ConnectorId;
using DRMTypes::ConnectorPtr;
using DRMTypes::CrtcId;
using DRMTypes::CrtcPtr;
using DRMTypes::EncoderId;
using DRMTypes::EncoderPtr;

namespace {
void reportSystemError(const char* operation, int error) noexcept
{
    std::fprintf(stderr, "%s: %s\n", operation, std::strerror(error));
}
}

DrmDevice::FileDescriptor::~FileDescriptor() noexcept
{
    reset();
}

void DrmDevice::FileDescriptor::reset(int value) noexcept
{
    const int previous = std::exchange(value_, value);
    if (previous >= 0 && close(previous) != 0) {
        // On Linux the descriptor must not be retried after close(), since
        // its number may already have been reused, even when close fails.
        reportSystemError("Could not close DRM device", errno);
    }
}

DrmDevice::DrmDevice() noexcept
    : bufferAccess_(*this),
      buffers_{DrmBuffer(*this), DrmBuffer(*this)}
{
}

DrmDevice::~DrmDevice() noexcept
{
    reset();
}

bool DrmDevice::reset() noexcept
{
    bool released = true;
    for (auto& buffer : buffers_) {
        if (!buffer.destroyBuffer()) {
            released = false;
        }
    }

    if (!released) {
        // Keep the connection valid for unresolved releases and a retry.
        return false;
    }

    resources_.reset();
    selectedDisplay_ = {};
    savedCrtc_ = {};
    fd_.reset();
    return true;
}

bool DrmDevice::shutdown() noexcept
{
    return reset();
}

bool DrmDevice::initialize(const char* path)
{
    if (fd_.get() >= 0) {
        std::cerr << "DrmDevice is already open; shut it down before reinitializing.\n";
        return false;
    }

    try {
        const bool initialized =
            openDevice(path) &&
            acquireMaster() &&
            loadResources() &&
            selectAndSaveDisplay() &&
            createBuffers();

        if (!initialized) {
            reset();
        }

        return initialized;
    } catch (...) {
        reset();
        throw;
    }
}

bool DrmDevice::openDevice(const char* path)
{
    fd_.reset(open(path, O_RDWR | O_CLOEXEC));

    if (fd_.get() < 0) {
        std::cerr << "Could not open " << path << ": "
                  << std::strerror(errno) << '\n';
        return false;
    }

    std::cout << "Opened " << path
              << " under fd " << fd_.get() << '\n';

    return true;
}

bool DrmDevice::acquireMaster()
{
    if (!drmIsMaster(fd_.get()) && drmSetMaster(fd_.get()) != 0) {
        std::cerr << "Could not become DRM master: "
                  << std::strerror(errno) << '\n';
        return false;
    }

    return true; 
}

bool DrmDevice::loadResources()
{
    resources_.reset(drmModeGetResources(fd_.get()));

    if (resources_ == nullptr) {
        std::cerr << "Could not get DRM resources: "
                  << std::strerror(errno) << '\n';
        return false;
    }

    return true;
}

const DisplaySelection& DrmDevice::selectedDisplay() const
{
    assert(selectedDisplay_.connectorId != 0);
    return selectedDisplay_;
}

const DrmDevice::BufferAccess& DrmDevice::bufferAccess() const noexcept
{
    return bufferAccess_;
}

drmModeModeInfo DrmDevice::chooseMode(const drmModeConnector& connector)
{
    // Prefer a reported progressive 1920x1080 mode at exactly 60 Hz.
    for (int i = 0; i < connector.count_modes; ++i) {
        const drmModeModeInfo& mode = connector.modes[i];

        if (mode.hdisplay == 1920 &&
            mode.vdisplay == 1080 &&
            mode.htotal != 0 &&
            mode.vtotal != 0 &&
            mode.vscan <= 1 &&
            !(mode.flags &
              (DRM_MODE_FLAG_INTERLACE | DRM_MODE_FLAG_DBLSCAN)) &&
            std::uint64_t{mode.clock} * 1000 ==
                std::uint64_t{60} * mode.htotal * mode.vtotal) {
            return mode;
        }
    }

    // No reported mode matched: prepare standard 1080p60 timings.
    // The eventual modeset may still reject this mode.
    drmModeModeInfo mode{};
    mode.clock = 148500;
    mode.hdisplay = 1920;
    mode.hsync_start = 2008;
    mode.hsync_end = 2052;
    mode.htotal = 2200;
    mode.vdisplay = 1080;
    mode.vsync_start = 1084;
    mode.vsync_end = 1089;
    mode.vtotal = 1125;
    mode.vrefresh = 60;
    mode.flags = DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC;
    mode.type = DRM_MODE_TYPE_USERDEF;
    std::strcpy(mode.name, "1920x1080");

    return mode;
}

bool DrmDevice::saveCrtcState(CrtcId crtcId)
{
    CrtcPtr crtc(drmModeGetCrtc(fd_.get(), crtcId), drmModeFreeCrtc);

    if (!crtc) {
        std::cerr << "Could not inspect CRTC " << crtcId << ": "
                  << std::strerror(errno) << '\n';
        return false;
    }

    const drmModeCrtc& current = *crtc;

    // A previous HDMI candidate may have used this member. Start its
    // connector list empty before recording the newly chosen CRTC.
    savedCrtc_.connectorIds.clear();

    savedCrtc_.crtcId = crtcId;
    savedCrtc_.framebufferId = current.buffer_id;
    savedCrtc_.x = current.x;
    savedCrtc_.y = current.y;
    savedCrtc_.modeValid = current.mode_valid != 0;
    savedCrtc_.mode = savedCrtc_.modeValid
        ? current.mode
        : drmModeModeInfo{};

    return true;
}

bool DrmDevice::selectAndSaveDisplay()
{
    struct HdmiCandidate {
        ConnectorId connectorId{};
        EncoderId activeEncoderId{};
        CrtcId activeCrtcId{};
        // the DRM kernel API exposes possible CRTCs via a bitmask
        std::uint32_t encoderToCrtcMask{};  
    };

    if (fd_.get() < 0 || resources_ == nullptr) {
        std::cerr
            << "DRM device is not ready for display selection.\n";
        return false;
    }

    ConnectorPtr connector(nullptr, drmModeFreeConnector);
    EncoderPtr encoder(nullptr, drmModeFreeEncoder);

    std::vector<std::pair<ConnectorId, CrtcId>> routes;
    // Physical HDMI Ports
    std::vector<HdmiCandidate> hdmiCandidates;
    bool routesComplete = true;

    // First pass: discover existing routes and identify HDMI outputs.
    for (int i = 0; i < resources_->count_connectors; ++i) {
       
        const ConnectorId connectorId =
            resources_->connectors[i];

        connector.reset(
            drmModeGetConnectorCurrent(fd_.get(), connectorId)
        );

        if (!connector) {
            std::cerr << "Could not inspect connector "
                      << connectorId << ": "
                      << std::strerror(errno) << '\n';
            routesComplete = false;
            continue;
        }

        // Observe if the current connector is of type HDMI.
        // If so, we extract the encoder that is bound to the connector. 
        const drmModeConnector& current = *connector;
        const bool isHdmi =
            current.connector_type == DRM_MODE_CONNECTOR_HDMIA ||
            current.connector_type == DRM_MODE_CONNECTOR_HDMIB;
        const EncoderId activeEncoderId = current.encoder_id;

        CrtcId activeCrtcId{};
        std::uint32_t encoderToCrtcMask{};

        encoder.reset();

        if (activeEncoderId != 0) {
            encoder.reset(
                drmModeGetEncoder(fd_.get(), activeEncoderId)
            );

            if (!encoder) {
                std::cerr << "Could not inspect encoder "
                          << activeEncoderId << ": "
                          << std::strerror(errno) << '\n';
                routesComplete = false;
            } else {
                const drmModeEncoder& active = *encoder;

                // Copy these while the query result is still owned.
                activeCrtcId = active.crtc_id;
                encoderToCrtcMask = active.possible_crtcs;

                if (activeCrtcId != 0) {
                    routes.emplace_back(
                        connectorId, activeCrtcId
                    );
                }
            }
        }

        if (isHdmi) {

            hdmiCandidates.push_back({
                connectorId,
                activeEncoderId,
                activeCrtcId,
                encoderToCrtcMask

                
            });
        }
    }

    // Second pass: inspect HDMI's full capabilities and pick a CRTC.
    for (const HdmiCandidate& candidate : hdmiCandidates) {
        connector.reset(
            drmModeGetConnector(fd_.get(), candidate.connectorId)
        );

        if (!connector) {
            std::cerr << "Could not probe HDMI connector "
                      << candidate.connectorId << ": "
                      << std::strerror(errno) << '\n';
            continue;
        }

        const drmModeConnector& hdmi = *connector;  
        CrtcId chosenCrtcId = candidate.activeCrtcId;

        // If the connector's encoder's CTRC is not assigned...
        if (chosenCrtcId == 0) {
            CrtcId occupiedFallback = 0;

            for (int e = 0;
                 e < hdmi.count_encoders && chosenCrtcId == 0;
                 ++e) {
                const EncoderId encoderId = hdmi.encoders[e];
                std::uint32_t possibleCrtcsMask = 0;

                // Reuse the first pass's result when available.
                if (encoderId == candidate.activeEncoderId &&
                    candidate.encoderToCrtcMask != 0) {
                    possibleCrtcsMask =
                        candidate.encoderToCrtcMask;
                // Alternative encoder choice
                } else {
                    encoder.reset(
                        drmModeGetEncoder(fd_.get(), encoderId)
                    );

                    if (!encoder) {
                        continue;
                    }

                    possibleCrtcsMask = encoder->possible_crtcs;
                }

                for (int c = 0;
                     c < resources_->count_crtcs && c < 32;
                     ++c) {
                    if ((possibleCrtcsMask &
                         (std::uint32_t{1} << c)) == 0) {
                        continue;
                    }

                    const CrtcId crtcId = resources_->crtcs[c];

                    const bool occupied = std::any_of(
                        routes.begin(),
                        routes.end(),
                        [crtcId](const auto& route) {
                            return route.second == crtcId;
                        }
                    );

                    if (!occupied) {
                        chosenCrtcId = crtcId;
                        break;
                    }

                    if (occupiedFallback == 0) {
                        occupiedFallback = crtcId;
                    }
                }
            }

            // Try an occupied route if no free compatible one exists.
            if (chosenCrtcId == 0) {
                chosenCrtcId = occupiedFallback;
            }
        }

        if (chosenCrtcId == 0) {
            std::cerr << "HDMI connector "
                      << candidate.connectorId
                      << " has no compatible CRTC.\n";
            continue;
        }

        if (!saveCrtcState(chosenCrtcId)) {
            continue;
        }

        // With an active CRTC, an incomplete first pass could
        // leave out one of its original attached connectors.
        if (savedCrtc_.modeValid && !routesComplete) {
            std::cerr
                << "Cannot safely save all connectors attached "
                   "to the active CRTC.\n";
            continue;
        }

        for (const auto& [connectorId, crtcId] : routes) {
            if (crtcId == chosenCrtcId) {
                savedCrtc_.connectorIds.push_back(connectorId);
            }
        }

        selectedDisplay_ = {
            candidate.connectorId,
            chosenCrtcId,
            chooseMode(hdmi)
        };

        return true;
    }

    std::cerr
        << "Could not find an HDMI connector with a usable CRTC.\n";
    return false;
}

bool DrmDevice::createBuffers()
{
    if (!buffers_[0].createBuffer())
        return false;

    if (!buffers_[1].createBuffer()) {
        buffers_[0].destroyBuffer();
        return false;
    }

    return true;
}

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
