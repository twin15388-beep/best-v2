#pragma once

#include "physics/snapshot.h"

#include <cstdint>
#include <memory>

namespace avoidfreeze::win32
{
class Win32Reader final
{
    struct Impl;
    std::unique_ptr<Impl> m_Impl;

public:
    explicit Win32Reader(std::uint32_t process_id);
    ~Win32Reader();
    Win32Reader(Win32Reader &&) noexcept;
    Win32Reader &operator=(Win32Reader &&) noexcept;
    Win32Reader(const Win32Reader &) = delete;
    Win32Reader &operator=(const Win32Reader &) = delete;

    SnapshotFrame Capture();
};
} // namespace avoidfreeze::win32
