// SPDX-License-Identifier: MIT
#pragma once
#include "../win32.h"
#include "policy.h"

namespace malinka::avoid
{
class LiveReader
{
    helper::Handle process_;
    DWORD pid_{};
    std::uint64_t root_{},module_{},creation_{},generation_{};
    int last_tick_{-1};
    std::shared_ptr<const StaticMap> map_;
    std::vector<std::uint8_t> Read(std::uint64_t address,std::size_t count) const;
    int Int(std::uint64_t address) const;
    std::uint64_t Pointer(std::uint64_t address) const;
    bool ExactBinding(int key,std::string_view command) const;
public:
    explicit LiveReader(const helper::Report &report);
    bool Alive() const;
    DWORD Pid() const {return pid_;}
    std::uint64_t Creation() const {return creation_;}
    // An absent result means the same already-sampled prediction tick; it is
    // not permission to keep keys held indefinitely. The host has a timeout.
    std::optional<Frame> Capture();
    bool ArrowLeftBound() const;
    bool ArrowRightBound() const;
};
void ValidateNativeReplicaAbi();
} // namespace malinka::avoid
