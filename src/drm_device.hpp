#pragma once

#include "drm_buffer.hpp"
#include "frame_view.hpp"

#include <cstddef>
#include <cstdint>
#include <vector>
#include <xf86drmMode.h>
#include <memory>
#include <array> 
#include <optional>

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

using DRMTypes::ConnectorId;
using DRMTypes::ConnectorPtr;
using DRMTypes::CrtcId;

struct DisplaySelection {
    ConnectorId connectorId{};
    CrtcId crtcId{};
    drmModeModeInfo mode{};
};

class DrmDevice {
public:

    // DrmBuffer's isolated view into DrmDevice.
    // DrmDevice owns DrmBuffer's RAII, but DrmBuffer 
    // needs DrmDevice's KMS functions to init.
    class BufferAccess {
    public:
    
        struct Dimensions {
            std::uint32_t width{};
            std::uint32_t height{};
        };

        Dimensions dimensions() const noexcept;

        BufferAccess(const BufferAccess&) = delete;
        BufferAccess& operator=(const BufferAccess&) = delete;
        BufferAccess(BufferAccess&&) = delete;
        BufferAccess& operator=(BufferAccess&&) = delete;

    private:
        friend class DrmDevice;
        friend class DrmBuffer;

        struct Allocation {
            std::uint32_t handle{};
            std::uint32_t pitch{};
            std::uint64_t size{};
        };

        bool allocateDumbBuffer(std::uint32_t width, std::uint32_t height,
                                std::uint32_t bpp, Allocation& allocation) const noexcept;
        bool registerFramebuffer(std::uint32_t width, std::uint32_t height,
                                 std::uint32_t format, std::uint32_t handle,
                                 std::uint32_t pitch, std::uint32_t& id) const noexcept;
        bool mappingOffset(std::uint32_t handle, std::uint64_t& offset) const noexcept;
        void* mapBuffer(std::size_t size, std::uint64_t offset) const noexcept;
        bool removeFramebuffer(std::uint32_t id) const noexcept;
        bool destroyDumbBuffer(std::uint32_t handle) const noexcept;
        bool canReleaseBuffer() const noexcept;

        explicit BufferAccess(const DrmDevice& device) noexcept;
        const DrmDevice& device_;
    };

    DrmDevice() noexcept;
    ~DrmDevice() noexcept;

    DrmDevice(const DrmDevice&) = delete;
    DrmDevice& operator=(const DrmDevice&) = delete;

    DrmDevice(DrmDevice&&) = delete;
    DrmDevice& operator=(DrmDevice&&) = delete;

    bool initialize(const char* path);
    bool shutdown() noexcept;

    // QA
    const DisplaySelection& selectedDisplay() const;
    
    const BufferAccess& bufferAccess() const noexcept;

    std::optional<FrameView> acquireFrame() noexcept;
    bool present(const FrameView& frame) noexcept;
    int eventFd() const noexcept { return fd_.get(); }
    bool processEvents() noexcept;
    bool connected() const noexcept;

    // QA - Completion status of acquired Buffer (defined below)
    bool flipPending() const noexcept { return flipPending_; }

    enum class BufferRole { Unmapped, Available, Front, Pending, Drawing };

    struct BufferInfo {
        std::uintptr_t address{};
        std::size_t bytes{};
        std::uint32_t pitch{};
        BufferRole role = BufferRole::Unmapped;
    };
   
    std::array<BufferInfo, 2> bufferInfo() const noexcept;

private:

    class FileDescriptor {
    public:
        FileDescriptor() = default;
        ~FileDescriptor() noexcept;

        FileDescriptor(const FileDescriptor&) = delete;
        FileDescriptor& operator=(const FileDescriptor&) = delete;
        FileDescriptor(FileDescriptor&&) = delete;
        FileDescriptor& operator=(FileDescriptor&&) = delete;

        int get() const noexcept { return value_; }
        void reset(int value = -1) noexcept;

    private:
        int value_ = -1;
    };

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
    bool reset() noexcept;
    bool restoreDisplay() noexcept;
    static drmModeModeInfo chooseMode(const drmModeConnector& connector);
    bool createBuffers(); 

    FileDescriptor fd_{};

    std::unique_ptr<drmModeRes, decltype(&drmModeFreeResources)> resources_{
        nullptr, drmModeFreeResources
    };
    
    DisplaySelection selectedDisplay_{};
    SavedCrtcState savedCrtc_{};
    bool initialized_{};
    bool frameAcquired_{};
    bool displayActive_{};

    // Status of acquired Buffer
    // Index of buffer being displayed 
    // Index of buffer in queue (drawing or waiting)
    bool flipPending_{};
    std::size_t front_{};
    std::size_t acquired_{};

    static void flipped(int, unsigned, unsigned, unsigned, void*) noexcept;

    BufferAccess bufferAccess_;
    std::array<DrmBuffer, 2> buffers_;
};
