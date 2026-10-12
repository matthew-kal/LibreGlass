#pragma once

#include <array>
#include <condition_variable>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

struct PhotoFile {
    std::string path;
    std::uintmax_t bytes{};
    std::filesystem::file_time_type modified{};
    bool operator==(const PhotoFile&) const = default;
};

using PhotoList = std::vector<PhotoFile>;

struct PhotoImage {
    PhotoFile file;
    static constexpr unsigned width = 316;
    static constexpr unsigned height = 596;
    std::vector<std::uint32_t> pixels;
};

struct PhotoSnapshot {
    std::shared_ptr<const PhotoList> files;
    std::array<std::shared_ptr<const PhotoImage>, 2> images;
    bool failed{};
};

std::shared_ptr<const PhotoImage> loadPhoto(const PhotoFile& file);
std::filesystem::path defaultPhotoDirectory(const std::filesystem::path& executable);

class PhotoLoader {
public:
    explicit PhotoLoader(std::filesystem::path directory);
    ~PhotoLoader();
    PhotoLoader(const PhotoLoader&) = delete;
    PhotoLoader& operator=(const PhotoLoader&) = delete;
    void start();
    void stop() noexcept;
    int eventFd() const noexcept { return eventFd_; }
    std::optional<PhotoSnapshot> take() noexcept;
    void request(PhotoFile current, PhotoFile next);

private:
    void run(std::stop_token stop) noexcept;
    void publish(PhotoSnapshot snapshot) noexcept;
    std::filesystem::path directory_;
    int eventFd_ = -1;
    std::mutex mutex_;
    std::condition_variable ready_;
    std::optional<std::array<PhotoFile, 2>> request_;
    std::optional<PhotoSnapshot> pending_;
    std::jthread thread_;
};
