#pragma once

#include <cstdint>
#include <optional>
#include <vector>
#include <xf86drmMode.h>
#include <memory>

struct DisplaySelection {
    std::uint32_t connectorId{};
    std::uint32_t crtcId{};
    drmModeModeInfo mode{};
};

namespace DRMTypes{

    using CrtcId = 
        std::uint32_t;
    using CrtcPtr = 
        std::unique_ptr<drmModeCrtc, decltype(&drmModeFreeCrtc)>;

    using ConnectorId =
        std::uint32_t;
    using ConnectorPtr = 
        std::unique_ptr<drmModeConnector, decltype(&drmModeFreeConnector)>;

    using EncoderId = 
        std::uint32_t;
    using EncoderPtr = 
        std::unique_ptr<drmModeEncoder, decltype(&drmModeFreeEncoder)>;
        
}

class DrmDevice {
public:
    DrmDevice() = default;
    ~DrmDevice();

    DrmDevice(const DrmDevice&) = delete;
    DrmDevice& operator=(const DrmDevice&) = delete;

    DrmDevice(DrmDevice&&) = delete;
    DrmDevice& operator=(DrmDevice&&) = delete;

    bool initialize(const char* path);
    const DisplaySelection& selectedDisplay() const;

private:
    struct SavedCrtcState {
        std::uint32_t crtcId{};
        std::uint32_t framebufferId{};
        std::uint32_t x{};
        std::uint32_t y {};
        bool modeValid{false};
        drmModeModeInfo mode{};
        std::vector<std::uint32_t> connectorIds;
    };

    bool openDevice(const char* path);
    bool acquireMaster();
    bool loadResources();
    bool selectAndSaveDisplay();
    bool saveCrtcState(CrtcId crtcId);
    void reset();
    static drmModeModeInfo chooseMode(const drmModeConnector& connector);

    int fd_ = -1;
    drmModeRes* resources_ = nullptr;
    DisplaySelection selectedDisplay_{};
    SavedCrtcState savedCrtc_{};
};