#pragma once

#include <cstdint>
#include <utility>

// Observations belong to the calling thread. The process panel accepts them only
// when they fall inside the main thread's [stack] mapping.
namespace StackDepth {
inline thread_local std::uintptr_t lowestPointer{};

[[gnu::always_inline]] inline std::uintptr_t currentPointer() noexcept
{
    std::uintptr_t pointer{};
    asm volatile("mov %%rsp, %0" : "=r"(pointer));
    return pointer; // Unsupported architectures report unavailable.
}

//QA
// The explicit pointer argument also lets tests supply known stack positions.
inline void observe(std::uintptr_t pointer = currentPointer()) noexcept
{
    if (pointer && (!lowestPointer || pointer < lowestPointer))
        lowestPointer = pointer;
}

inline std::uintptr_t takeLowest() noexcept
{
    return std::exchange(lowestPointer, 0);
}
}
