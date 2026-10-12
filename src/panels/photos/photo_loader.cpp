#include "photo_loader.hpp"

#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <fstream>
#include <span>
#include <stdexcept>
#include <sys/eventfd.h>
#include <unistd.h>

struct _GdkPixbuf;
struct _GError;
using GdkPixbuf = _GdkPixbuf;
using GError = _GError;

extern "C" {
GdkPixbuf* gdk_pixbuf_new_from_file_at_scale(const char*, int, int, int, GError**);
GdkPixbuf* gdk_pixbuf_apply_embedded_orientation(GdkPixbuf*);
GdkPixbuf* gdk_pixbuf_scale_simple(const GdkPixbuf*, int, int, int);
int gdk_pixbuf_get_width(const GdkPixbuf*);
int gdk_pixbuf_get_height(const GdkPixbuf*);
int gdk_pixbuf_get_n_channels(const GdkPixbuf*);
int gdk_pixbuf_get_bits_per_sample(const GdkPixbuf*);
int gdk_pixbuf_get_rowstride(const GdkPixbuf*);
int gdk_pixbuf_set_option(GdkPixbuf*, const char*, const char*);
int gdk_pixbuf_remove_option(GdkPixbuf*, const char*);
unsigned char* gdk_pixbuf_get_pixels(const GdkPixbuf*);
void g_object_unref(void*);
}

namespace {
using Pixbuf = std::unique_ptr<GdkPixbuf, decltype(&g_object_unref)>;

unsigned exifOrientation(std::span<const unsigned char> bytes) {
    if (bytes.size() < 8) return 1;
    const bool little = bytes[0] == 'I' && bytes[1] == 'I';
    if (!little && !(bytes[0] == 'M' && bytes[1] == 'M')) return 1;
    const auto word = [&](std::size_t offset) -> unsigned {
        return little ? unsigned(bytes[offset]) | unsigned(bytes[offset+1]) << 8 :
                        unsigned(bytes[offset]) << 8 | bytes[offset+1];
    };
    const auto number = [&](std::size_t offset) -> std::uint32_t {
        return little ? std::uint32_t(word(offset)) | std::uint32_t(word(offset+2)) << 16 :
                        std::uint32_t(word(offset)) << 16 | word(offset+2);
    };
    if (word(2) != 42) return 1;
    const std::size_t offset = number(4);
    if (offset > bytes.size() || bytes.size() - offset < 2) return 1;
    const auto count = word(offset);
    if (count > (bytes.size() - offset - 2) / 12) return 1;
    for (unsigned i = 0; i < count; ++i) {
        const auto entry = offset + 2 + std::size_t(i) * 12;
        if (word(entry) == 0x112 && word(entry+2) == 3 && number(entry+4) == 1) {
            const auto orientation = word(entry+8);
            return orientation >= 1 && orientation <= 8 ? orientation : 1;
        }
    }
    return 1;
}

std::optional<unsigned> pngOrientation(const PhotoFile& file) {
    std::array<unsigned char, 24> header{};
    std::ifstream input(file.path, std::ios::binary);
    if (!input.read(reinterpret_cast<char*>(header.data()), header.size())) return {};
    constexpr std::array<unsigned char, 8> signature{137, 80, 78, 71, 13, 10, 26, 10};
    if (!std::equal(signature.begin(), signature.end(), header.begin()) ||
        header[12] != 'I' || header[13] != 'H' || header[14] != 'D' || header[15] != 'R')
        return {};
    const auto number = [&](unsigned offset) {
        return (std::uint32_t(header[offset]) << 24) | (std::uint32_t(header[offset+1]) << 16) |
               (std::uint32_t(header[offset+2]) << 8) | header[offset+3];
    };
    const auto width = number(16), height = number(20);
    if (!width || !height || std::uint64_t(width) * height > 64ull * 1024 * 1024) return {};
    input.seekg(8);
    for (unsigned chunks = 0; chunks < 4096; ++chunks) {
        if (!input.read(reinterpret_cast<char*>(header.data()), 8)) break;
        const auto length = number(0);
        if (length > 0x7fffffff) break;
        if (header[4] == 'e' && header[5] == 'X' && header[6] == 'I' && header[7] == 'f') {
            if (length > 1024 * 1024) return 1;
            std::vector<unsigned char> bytes(length);
            if (!input.read(reinterpret_cast<char*>(bytes.data()), length)) return 1;
            return exifOrientation(bytes);
        }
        if (header[4] == 'I' && header[5] == 'E' && header[6] == 'N' && header[7] == 'D') break;
        input.seekg(std::streamoff(length) + 4, std::ios::cur);
    }
    return 1;
}

PhotoList scan(const std::filesystem::path& directory) {
    PhotoList files;
    std::error_code error;
    std::filesystem::directory_iterator iterator(directory, error), end;
    while (!error && iterator != end) {
        const auto& entry = *iterator;
        if (entry.is_regular_file(error) && !error) {
            auto extension = entry.path().extension().string();
            std::transform(extension.begin(), extension.end(), extension.begin(), [](unsigned char c) {
                return c >= 'A' && c <= 'Z' ? char(c + 'a' - 'A') : char(c);
            });
            if (extension == ".png") {
                PhotoFile file{entry.path().string(), entry.file_size(error), {}};
                if (!error) file.modified = entry.last_write_time(error);
                if (!error) files.push_back(std::move(file));
            }
        }
        if (error) break;
        iterator.increment(error);
    }
    if (error && error != std::errc::no_such_file_or_directory)
        throw std::filesystem::filesystem_error("Could not scan photos", directory, error);
    std::sort(files.begin(), files.end(), [](const PhotoFile& a, const PhotoFile& b) {
        return a.path < b.path;
    });
    return files;
}
}

std::filesystem::path defaultPhotoDirectory(const std::filesystem::path& executable)
{
    std::error_code error;
    auto resolved = std::filesystem::canonical(executable, error);
    if (error) resolved = std::filesystem::absolute(executable).lexically_normal();
    const auto executableDirectory = resolved.parent_path();
    auto fallback = executableDirectory / "photos";
    for (auto directory = executableDirectory; !directory.empty();) {
        const auto candidate = directory / "photos";
        if (std::filesystem::is_directory(candidate, error)) return candidate;
        if (directory.filename() == "build") fallback = directory.parent_path() / "photos";
        const auto parent = directory.parent_path();
        if (parent == directory) break;
        directory = parent;
    }
    return fallback;
}

std::shared_ptr<const PhotoImage> loadPhoto(const PhotoFile& file)
{
    const auto orientation = pngOrientation(file);
    if (!orientation) return {};
    const bool swap = *orientation >= 5;
    Pixbuf decoded(gdk_pixbuf_new_from_file_at_scale(file.path.c_str(),
                   swap ? PhotoImage::height : PhotoImage::width,
                   swap ? PhotoImage::width : PhotoImage::height, 1, nullptr), g_object_unref);
    if (!decoded) return {};
    const char value[]{char('0' + *orientation), 0};
    gdk_pixbuf_remove_option(decoded.get(), "orientation");
    gdk_pixbuf_set_option(decoded.get(), "orientation", value);
    Pixbuf oriented(gdk_pixbuf_apply_embedded_orientation(decoded.get()), g_object_unref);
    if (!oriented) return {};
    decoded.reset();
    auto width = gdk_pixbuf_get_width(oriented.get());
    auto height = gdk_pixbuf_get_height(oriented.get());
    if (width <= 0 || height <= width) return {};
    if (width > int(PhotoImage::width) || height > int(PhotoImage::height)) {
        const double factor = std::min(double(PhotoImage::width) / width,
                                       double(PhotoImage::height) / height);
        width = std::max(1, int(width * factor));
        height = std::max(1, int(height * factor));
        Pixbuf resized(gdk_pixbuf_scale_simple(oriented.get(), width, height, 2), g_object_unref);
        if (!resized) return {};
        oriented = std::move(resized);
    }
    const auto channels = gdk_pixbuf_get_n_channels(oriented.get());
    const auto stride = gdk_pixbuf_get_rowstride(oriented.get());
    const auto* source = gdk_pixbuf_get_pixels(oriented.get());
    if (!source || (channels != 3 && channels != 4) ||
        gdk_pixbuf_get_bits_per_sample(oriented.get()) != 8 || stride < width * channels)
        return {};
    auto image = std::make_shared<PhotoImage>();
    image->file = file;
    image->pixels.resize(std::size_t(PhotoImage::width) * PhotoImage::height);
    const auto left = (PhotoImage::width - unsigned(width)) / 2;
    const auto top = (PhotoImage::height - unsigned(height)) / 2;
    for (int y = 0; y < height; ++y) {
        const auto* row = source + std::size_t(y) * stride;
        auto* destination = image->pixels.data() + std::size_t(top + y) * PhotoImage::width + left;
        for (int x = 0; x < width; ++x) {
            const auto* pixel = row + x * channels;
            const unsigned alpha = channels == 4 ? pixel[3] : 255;
            destination[x] = ((unsigned(pixel[0]) * alpha + 127) / 255 << 16) |
                             ((unsigned(pixel[1]) * alpha + 127) / 255 << 8) |
                              (unsigned(pixel[2]) * alpha + 127) / 255;
        }
    }
    return image;
}

PhotoLoader::PhotoLoader(std::filesystem::path directory)
    : directory_(std::move(directory)), eventFd_(eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK))
{
}

PhotoLoader::~PhotoLoader()
{
    stop();
    if (eventFd_ >= 0) close(eventFd_);
}

void PhotoLoader::start()
{
    if (eventFd_ < 0) throw std::runtime_error("Could not create photo notification descriptor");
    if (!thread_.joinable()) thread_ = std::jthread([this](std::stop_token stop) { run(stop); });
}

void PhotoLoader::stop() noexcept
{
    if (!thread_.joinable()) return;
    thread_.request_stop();
    {
        std::lock_guard lock(mutex_);
    }
    ready_.notify_one();
    thread_.join();
}

void PhotoLoader::request(PhotoFile current, PhotoFile next)
{
    std::array<PhotoFile, 2> files{std::move(current), std::move(next)};
    {
        std::lock_guard lock(mutex_);
        request_ = std::move(files);
    }
    ready_.notify_one();
}

void PhotoLoader::publish(PhotoSnapshot snapshot) noexcept
{
    std::optional<PhotoSnapshot> replaced;
    {
        std::lock_guard lock(mutex_);
        replaced.swap(pending_);
        pending_ = std::move(snapshot);
    }
    const std::uint64_t one = 1;
    ssize_t written;
    do { written = write(eventFd_, &one, sizeof(one)); } while (written < 0 && errno == EINTR);
    if (written < 0 && errno != EAGAIN) std::perror("Could not notify photo panel");
}

std::optional<PhotoSnapshot> PhotoLoader::take() noexcept
{
    std::uint64_t value;
    while (true) {
        const auto result = read(eventFd_, &value, sizeof(value));
        if (result >= 0 || errno == EINTR) continue;
        break;
    }
    std::optional<PhotoSnapshot> snapshot;
    {
        std::lock_guard lock(mutex_);
        snapshot.swap(pending_);
    }
    return snapshot;
}

void PhotoLoader::run(std::stop_token stop) noexcept
{
    PhotoSnapshot state;
    PhotoList failed;
    try {
        auto nextScan = std::chrono::steady_clock::time_point{};
        while (!stop.stop_requested()) {
            const auto now = std::chrono::steady_clock::now();
            if (now >= nextScan) {
                try {
                    auto files = scan(directory_);
                    std::erase_if(failed, [&](const PhotoFile& file) {
                        return std::find(files.begin(), files.end(), file) == files.end();
                    });
                    std::erase_if(files, [&](const PhotoFile& file) {
                        return std::find(failed.begin(), failed.end(), file) != failed.end();
                    });
                    const bool recovered = state.failed;
                    state.failed = false;
                    if (!state.files || *state.files != files) {
                        state.files = std::make_shared<const PhotoList>(std::move(files));
                        for (auto& image : state.images)
                            if (image && std::find(state.files->begin(), state.files->end(), image->file) == state.files->end())
                                image.reset();
                        publish(state);
                    } else if (recovered) publish(state);
                } catch (const std::filesystem::filesystem_error& error) {
                    if (!state.failed) std::fprintf(stderr, "Could not scan photos: %s\n", error.what());
                    state.failed = true;
                    publish(state);
                }
                nextScan = now + std::chrono::seconds(2);
            }
            std::optional<std::array<PhotoFile, 2>> request;
            {
                std::unique_lock lock(mutex_);
                ready_.wait_until(lock, nextScan, [&] { return stop.stop_requested() || request_.has_value(); });
                request.swap(request_);
            }
            if (stop.stop_requested()) break;
            if (!request) continue;
            auto previous = state.images;
            state.images = {};
            for (std::size_t i = 0; i < request->size() && !stop.stop_requested(); ++i) {
                const auto& file = (*request)[i];
                if (!state.files || std::find(state.files->begin(), state.files->end(), file) == state.files->end())
                    continue;
                for (const auto& image : previous)
                    if (image && image->file == file) state.images[i] = image;
                if (i && state.images[0] && state.images[0]->file == file) state.images[i] = state.images[0];
                if (!state.images[i]) state.images[i] = loadPhoto(file);
                if (!state.images[i]) {
                    std::fprintf(stderr, "Skipping PNG without a usable portrait image: %s\n", file.path.c_str());
                    failed.push_back(file);
                    nextScan = std::chrono::steady_clock::time_point{};
                }
                publish(state);
            }
        }
    } catch (const std::exception& error) {
        std::fprintf(stderr, "Photo loader failed: %s\n", error.what());
        state.failed = true;
        publish(std::move(state));
    } catch (...) {
        std::fputs("Photo loader failed.\n", stderr);
        state.failed = true;
        publish(std::move(state));
    }
}
