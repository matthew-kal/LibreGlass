#pragma once

#include <cstddef>
#include <cstdint>

struct FrameView {

// Non-owning view of a buffer (DrmBuffer)
// The owned portion of the buffer is unspecified. 
    
    std::uint8_t* pixels{}; 
    std::uint32_t width{}; 
    std::uint32_t height{};
    std::uint32_t pitch{};
    std::size_t size{}; 
    std::size_t bufferIndex{};

    bool valid() const noexcept
    {
        if (pixels == nullptr || width == 0 || height == 0) {
            return false;
        }

        const std::uint64_t rowBytes = std::uint64_t{width} * 4;
        return pitch >= rowBytes &&
               std::uint64_t{height - 1} * pitch + rowBytes <= size;
    }
};
