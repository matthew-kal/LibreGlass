#pragma once

#include <cstdint>
#include <optional>
#include <xf86drmMode.h>

class DrmDevice {
public:
    DrmDevice() = default; 
    ~DrmDevice();
    
    DrmDevice (const DrmDevice&) = delete;
    DrmDevice& operator=(const DrmDevice&) = delete;
    
    DrmDevice (DrmDevice&&) = delete;
    DrmDevice& operator=(DrmDevice&&) = delete; 

    bool initialize(const char* path); 
    std::optional<std::uint32_t> findConnectedHdmiConnector() const;

    private: 
        int fd_ = -1; 
        drmModeRes* resources_ = nullptr;
};
