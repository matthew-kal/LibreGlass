#include "drm_device.hpp"
#include "libdrm/drm_mode.h"

#include <algorithm>
#include <cassert>
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <utility>
#include <vector>
#include <unistd.h>
#include <xf86drm.h>

using DRMTypes::ConnectorId;
using DRMTypes::ConnectorPtr;
using DRMTypes::CrtcId;
using DRMTypes::CrtcPtr;
using DRMTypes::EncoderId;
using DRMTypes::EncoderPtr;

DrmDevice::~DrmDevice()
{
    reset();
}

void DrmDevice::reset()
{
    if (resources_ != nullptr) {
        drmModeFreeResources(resources_);
        resources_ = nullptr;
    }

    if (fd_ >= 0) {
        close(fd_);
        fd_ = -1;
    }

    // Reset object state when initialize() fails and the object may be reused.
    // The vector would free itself during destruction without this assignment.
    selectedDisplay_ = {};
    savedCrtc_ = {};
}

bool DrmDevice::initialize(const char* path)
{
    if (fd_ >= 0) {
        std::cerr << "DrmDevice is already initialized.\n";
        return false;
    }

    const bool initialized =
        openDevice(path) &&
        acquireMaster() &&
        loadResources() &&
        selectAndSaveDisplay();

    if (!initialized) {
        reset();
    }

    return initialized;
}

bool DrmDevice::openDevice(const char* path)
{
    fd_ = open(path, O_RDWR | O_CLOEXEC);

    if (fd_ < 0) {
        std::cerr << "Could not open " << path << ": "
                  << std::strerror(errno) << '\n';
        return false;
    }

    std::cout << "Opened " << path
              << " under fd " << fd_ << '\n';

    return true;
}

bool DrmDevice::acquireMaster()
{
    if (!drmIsMaster(fd_) && drmSetMaster(fd_) != 0) {
        std::cerr << "Could not become DRM master: "
                  << std::strerror(errno) << '\n';
        return false;
    }

    return true;
}

bool DrmDevice::loadResources()
{
    resources_ = drmModeGetResources(fd_);

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
    CrtcPtr crtc(drmModeGetCrtc(fd_, crtcId), drmModeFreeCrtc);

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

    if (fd_ < 0 || resources_ == nullptr) {
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
            drmModeGetConnectorCurrent(fd_, connectorId)
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
                drmModeGetEncoder(fd_, activeEncoderId)
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
            drmModeGetConnector(fd_, candidate.connectorId)
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
                        drmModeGetEncoder(fd_, encoderId)
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