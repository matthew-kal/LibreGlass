#pragma once
#include "../../panel.hpp"
#include <cstdint>

struct MemoryRegion {
    std::uint64_t bytes{};
    std::uint64_t low{};
    std::uint64_t high{};
};

struct KernelSnapshot {
    bool cpuValid{}, cpuReady{}, ramValid{}, processValid{}, mapsValid{}, faultsReady{};
    bool mapLimitValid{}, loadValid{}, uptimeValid{};
    bool stackDepthValid{};
    double cpuPercent{}, processCpuPercent{}, minorFaultsPerSecond{}, majorFaultsPerSecond{};
    double load{}, uptime{};
    std::uint64_t ramTotal{}, ramUsed{}, rss{}, virtualBytes{}, swap{};
    // Main mapping's upper boundary minus the deepest observed stack pointer.
    // Includes startup data, frame overhead and sampling; not resident RAM.
    std::uint64_t stackDepth{};
    unsigned pid{}, threads{}, mapCount{}, mapLimit{};
    MemoryRegion stack{}, heap{}, text{}, mappings{};
};

// Reads bounded procfs records once per sample. No smaps/page-table walk.
// root is injectable for parser tests; its storage must outlive this reader.
class KernelStats {
public:
    //QA
    explicit KernelStats(const char* root = "/proc") noexcept;
    KernelSnapshot sample(Panel::Clock::time_point now) noexcept;
private:
    //QA
    const char* root_;
    long ticksPerSecond_;
    bool previousCpu_{}, previousProcess_{};
    std::uint64_t total_{}, idle_{}, processTicks_{}, minor_{}, major_{};
    Panel::Clock::time_point previousTime_{};
};
