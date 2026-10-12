#include "../src/panels/photos/photo_panel.hpp"
#include "../src/renderer.hpp"

#include <cassert>
#include <cstdio>
#include <fstream>
#include <poll.h>
#include <span>
#include <unistd.h>

namespace fs = std::filesystem;
using Clock = Panel::Clock;
static_assert(PhotoPanel::index == 4);

const unsigned char redPng[]{137,80,78,71,13,10,26,10,0,0,0,13,73,72,68,82,0,0,0,2,0,0,0,3,8,6,0,0,0,185,234,222,129,0,0,0,17,73,68,65,84,120,156,99,248,207,192,240,31,132,25,48,24,0,161,121,11,245,77,196,154,7,0,0,0,0,73,69,78,68,174,66,96,130};
const unsigned char greenPng[]{137,80,78,71,13,10,26,10,0,0,0,13,73,72,68,82,0,0,0,2,0,0,0,3,8,6,0,0,0,185,234,222,129,0,0,0,14,73,68,65,84,120,156,99,96,248,15,133,24,12,0,155,127,11,245,40,20,176,4,0,0,0,0,73,69,78,68,174,66,96,130};
const unsigned char alphaPng[]{137,80,78,71,13,10,26,10,0,0,0,13,73,72,68,82,0,0,0,2,0,0,0,3,8,6,0,0,0,185,234,222,129,0,0,0,17,73,68,65,84,120,156,99,248,255,255,127,3,8,51,96,48,0,31,46,20,239,113,16,107,51,0,0,0,0,73,69,78,68,174,66,96,130};
const unsigned char portraitPng[]{137,80,78,71,13,10,26,10,0,0,0,13,73,72,68,82,0,0,0,2,0,0,0,3,8,2,0,0,0,54,136,73,214,0,0,0,22,73,68,65,84,120,156,99,248,207,192,192,0,194,64,226,255,127,48,241,159,1,0,78,198,8,248,71,49,138,92,0,0,0,0,73,69,78,68,174,66,96,130};
const unsigned char landscapePng[]{137,80,78,71,13,10,26,10,0,0,0,13,73,72,68,82,0,0,0,3,0,0,0,2,8,2,0,0,0,18,22,241,77,0,0,0,19,73,68,65,84,120,156,99,248,207,192,192,0,197,255,255,255,7,99,0,74,201,8,248,188,197,167,249,0,0,0,0,73,69,78,68,174,66,96,130};

std::vector<unsigned char> withExif(std::span<const unsigned char> png,
                                  std::span<const unsigned char> exif) {
    std::vector<unsigned char> chunk;
    const auto number = [&](std::uint32_t value) {
        for (int shift=24; shift>=0; shift-=8) chunk.push_back(static_cast<unsigned char>(value>>shift));
    };
    number(exif.size());
    chunk.insert(chunk.end(), {'e','X','I','f'});
    chunk.insert(chunk.end(), exif.begin(), exif.end());
    std::uint32_t crc=0xffffffff;
    for (std::size_t i=4; i<chunk.size(); ++i) {
        crc^=chunk[i];
        for (unsigned bit=0; bit<8; ++bit) crc=(crc>>1)^((crc&1) ? 0xedb88320 : 0);
    }
    number(crc^0xffffffff);
    std::vector<unsigned char> result(png.begin(), png.begin()+33);
    result.insert(result.end(), chunk.begin(), chunk.end());
    result.insert(result.end(), png.begin()+33, png.end());
    return result;
}

struct TemporaryDirectory {
    fs::path path;
    TemporaryDirectory() {
        char pattern[] = "/tmp/libreglass-photo-test-XXXXXX";
        const auto* directory = mkdtemp(pattern);
        assert(directory);
        path = directory;
    }
    ~TemporaryDirectory() { std::error_code error; fs::remove_all(path, error); }
};

void write(const fs::path& path, std::span<const unsigned char> bytes) {
    std::ofstream file(path, std::ios::binary);
    file.write(reinterpret_cast<const char*>(bytes.data()), bytes.size());
    assert(file.good());
}

PhotoFile record(const fs::path& path) {
    return {path.string(), fs::file_size(path), fs::last_write_time(path)};
}

template <class Predicate>
PhotoSnapshot waitFor(PhotoLoader& loader, Predicate predicate) {
    const auto deadline = Clock::now() + std::chrono::seconds(5);
    while (Clock::now() < deadline) {
        pollfd descriptor{loader.eventFd(), POLLIN, 0};
        const int result = poll(&descriptor, 1, 100);
        assert(result >= 0);
        if (!result) continue;
        auto snapshot = loader.take();
        if (snapshot && predicate(*snapshot)) return std::move(*snapshot);
    }
    std::fputs("Photo notification timed out\n", stderr);
    std::abort();
}

struct Frame {
    static constexpr unsigned pitch = 1920 * 4 + 8;
    static constexpr std::size_t size = std::size_t(pitch) * 1080;
    std::vector<unsigned char> bytes = std::vector<unsigned char>(size + 16, 0xa5);
    FrameView view{bytes.data()+8, 1920, 1080, pitch, size, 0};
    unsigned color(unsigned x, unsigned y) const {
        const auto bounds = Renderer::panelBounds(PhotoPanel::index);
        const auto* pixel = view.pixels + std::size_t(1080-bounds.x-x-1)*pitch + (bounds.y+y)*4;
        return unsigned(pixel[0]) | unsigned(pixel[1]) << 8 | unsigned(pixel[2]) << 16;
    }
    void guards() const {
        for (unsigned i=0; i<8; ++i) assert(bytes[i]==0xa5 && bytes[size+8+i]==0xa5);
        for (unsigned y=0; y<1080; ++y)
            for (unsigned x=1920*4; x<pitch; ++x)
                assert(view.pixels[std::size_t(y)*pitch+x]==0xa5);
    }
};

class ImagePanel final : public Panel {
public:
    static constexpr std::size_t index = PhotoPanel::index;
    bool update(Clock::time_point) noexcept override { return false; }
    void paint(RegionCanvas& canvas) const noexcept override {
        const std::array<std::uint32_t, 6> pixels{0xff0000,0x00ff00,0x0000ff,0xabcdef,0xfedcba,0x012345};
        canvas.image(-1, -1, 2, 3, pixels);
        canvas.image(10, 10, UINT32_MAX, UINT32_MAX, pixels);
        canvas.image(INT32_MAX, INT32_MAX, 2, 3, pixels);
        canvas.image(0, 0, 2, 3, pixels, 0);
    }
};

int main() {
    TemporaryDirectory temporary;
    TemporaryDirectory orientationDirectory;
    constexpr std::array<std::array<unsigned,4>,8> corners{{
        {0xff0000,0x00ff00,0xff00ff,0xffff00},
        {0x00ff00,0xff0000,0xffff00,0xff00ff},
        {0xffff00,0xff00ff,0x00ff00,0xff0000},
        {0xff00ff,0xffff00,0xff0000,0x00ff00},
        {0xff0000,0x00ffff,0x0000ff,0xffff00},
        {0x00ffff,0xff0000,0xffff00,0x0000ff},
        {0xffff00,0x0000ff,0x00ffff,0xff0000},
        {0x0000ff,0xffff00,0xff0000,0x00ffff}
    }};
    for (unsigned orientation=1; orientation<=8; ++orientation) {
        for (bool little : {false,true}) {
            std::vector<unsigned char> exif = little ?
                std::vector<unsigned char>{'I','I',42,0,8,0,0,0,1,0,18,1,3,0,1,0,0,0,
                                           static_cast<unsigned char>(orientation),0,0,0,0,0,0,0} :
                std::vector<unsigned char>{'M','M',0,42,0,0,0,8,0,1,1,18,0,3,0,0,0,1,0,
                                           static_cast<unsigned char>(orientation),0,0,0,0,0,0};
            const auto raw = orientation<5 ? std::span<const unsigned char>(portraitPng) :
                                            std::span<const unsigned char>(landscapePng);
            const auto path = orientationDirectory.path / "orientation.png";
            write(path, withExif(raw, exif));
            const auto image = loadPhoto(record(path));
            assert(image);
            const std::array<unsigned,4> actual{image->pixels[61*316],image->pixels[61*316+315],
                image->pixels[534*316],image->pixels[534*316+315]};
            assert(actual==corners[orientation-1]);
            assert(image->pixels[60*316]==0 && image->pixels[535*316]==0);
        }
    }
    const auto landscapePath = orientationDirectory.path / "landscape.png";
    write(landscapePath, landscapePng);
    assert(!loadPhoto(record(landscapePath)));
    const auto malformedPath = orientationDirectory.path / "malformed.png";
    const std::array<unsigned char,8> malformed{'M','M',0,42,255,255,255,255};
    write(malformedPath, withExif(portraitPng, malformed));
    assert(loadPhoto(record(malformedPath)));
    const auto project = temporary.path / "project";
    const auto executable = project / "build" / "debug" / "libre-glass";
    fs::create_directories(executable.parent_path());
    write(executable, {});
    assert(defaultPhotoDirectory(executable)==project / "photos");
    fs::create_directory(project / "photos");
    const auto previousDirectory = fs::current_path();
    fs::current_path(temporary.path);
    assert(defaultPhotoDirectory(executable)==project / "photos");
    assert(defaultPhotoDirectory("project/build/debug/libre-glass")==project / "photos");
    fs::create_symlink(executable, temporary.path / "mirror-link");
    assert(defaultPhotoDirectory("mirror-link")==project / "photos");
    fs::current_path(previousDirectory);
    write(temporary.path / "a.png", redPng);
    write(temporary.path / "b.PNG", greenPng);
    write(temporary.path / "alpha.png", alphaPng);
    const auto a = record(temporary.path / "a.png"), b = record(temporary.path / "b.PNG");
    const auto red = loadPhoto(a), green = loadPhoto(b);
    assert(red && green && red->pixels.size()==316*596);
    assert(red->pixels.front()==0 && red->pixels[61*316]==0xff0000);
    assert(red->pixels[534*316]==0xff0000 && red->pixels[535*316]==0);
    assert(green->pixels[61*316]==0x00ff00);
    const auto alpha = loadPhoto(record(temporary.path / "alpha.png"));
    assert(alpha && alpha->pixels[61*316]==0x808080);
    write(temporary.path / "bad.png", std::span(redPng, 10));
    assert(!loadPhoto(record(temporary.path / "bad.png")));

    PhotoLoader inactive{temporary.path};
    auto panel = std::make_unique<PhotoPanel>(inactive);
    auto& photos = *panel;
    Renderer renderer{std::move(panel)};
    auto files = std::make_shared<const PhotoList>(PhotoList{a,b});
    const auto epoch = Clock::time_point{};
    assert(photos.accept({files,{red,{}},false}, epoch));
    renderer.invalidate<PhotoPanel::index>();
    assert(photos.nextUpdate()==Clock::time_point::max());
    assert(!photos.accept({files,{red,green},false}, epoch));
    assert(photos.nextUpdate()==epoch+std::chrono::seconds(30));
    Frame frame;
    assert(renderer.render(frame.view) && frame.color(100,100)==0xff0000);
    const auto first = frame.bytes;
    Frame other;
    assert(renderer.paint(other.view) && frame.bytes==other.bytes);
    assert(!renderer.update<PhotoPanel::index>(epoch+std::chrono::seconds(29)));
    assert(!renderer.update<PhotoPanel::index>(epoch+std::chrono::seconds(30)));
    assert(renderer.update<PhotoPanel::index>(epoch+std::chrono::milliseconds(30250)));
    assert(renderer.render(frame.view) && frame.color(100,100)==0x807f00);
    assert(renderer.update<PhotoPanel::index>(epoch+std::chrono::milliseconds(30500)));
    assert(renderer.render(frame.view) && frame.color(100,100)==0x00ff00);
    assert(photos.nextUpdate()==Clock::time_point::max());
    assert(!photos.accept({files,{green,red},false}, epoch+std::chrono::milliseconds(30500)));
    assert(photos.nextUpdate()==epoch+std::chrono::milliseconds(60500));
    frame.guards();
    assert(first!=frame.bytes);

    Renderer clipped{std::make_unique<ImagePanel>()};
    Frame clip;
    assert(clipped.render(clip.view));
    assert(clip.color(0,0)==0xabcdef && clip.color(0,1)==0x012345);
    assert(clip.color(1,0)==0 && clip.color(0,2)==0);
    const auto bounds = Renderer::panelBounds(PhotoPanel::index);
    for (unsigned y=0; y<1080; ++y)
        for (unsigned x=0; x<1920; ++x) {
            const auto portraitX = 1079-y, portraitY = x;
            const auto* pixel = clip.view.pixels + std::size_t(y)*Frame::pitch + x*4;
            if (portraitX<bounds.x || portraitX>=bounds.x+bounds.width ||
                portraitY<bounds.y || portraitY>=bounds.y+bounds.height)
                assert(!pixel[0] && !pixel[1] && !pixel[2]);
        }
    clip.guards();

    PhotoLoader loader{temporary.path};
    loader.start();
    auto initial = waitFor(loader, [](const PhotoSnapshot& snapshot) { return snapshot.files && snapshot.files->size()==4; });
    assert((*initial.files)[0]==a);
    loader.request(a,b);
    auto loaded = waitFor(loader, [](const PhotoSnapshot& snapshot) { return snapshot.images[0] && snapshot.images[1]; });
    assert(loaded.images[0]->file==a && loaded.images[1]->file==b);
    loader.request(record(temporary.path / "bad.png"), a);
    auto filtered = waitFor(loader, [](const PhotoSnapshot& snapshot) { return snapshot.files && snapshot.files->size()==3; });
    for (const auto& file : *filtered.files) assert(fs::path(file.path).filename()!="bad.png");
    write(temporary.path / "bad.png", redPng);
    auto repaired = waitFor(loader, [](const PhotoSnapshot& snapshot) { return snapshot.files && snapshot.files->size()==4; });
    assert(repaired.files->size()==4);
    loader.stop();
    const auto stopped = Clock::now();
    loader.stop();
    assert(Clock::now()-stopped<std::chrono::seconds(1));

    PhotoLoader missing{temporary.path / "missing"};
    missing.start();
    waitFor(missing, [](const PhotoSnapshot& snapshot) { return snapshot.files && snapshot.files->empty(); });
    fs::create_directory(temporary.path / "missing");
    write(temporary.path / "missing" / "new.png", redPng);
    waitFor(missing, [](const PhotoSnapshot& snapshot) { return snapshot.files && snapshot.files->size()==1; });
    missing.stop();
    std::puts("Passed PNG orientation, portrait-only display, centering, fitting, alpha, fades, clipping and loader recovery.");
}
