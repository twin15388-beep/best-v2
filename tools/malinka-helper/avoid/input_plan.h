// SPDX-License-Identifier: MIT
#pragma once
#include <optional>
namespace malinka::avoid
{
struct PhysicalKeys
{
    bool left{},right{};
    bool operator==(const PhysicalKeys &) const=default;
    int Direction() const {return static_cast<int>(right)-static_cast<int>(left);}
};
struct ExtraKeys {bool left{},right{};};
// Never synthesize a key-up of human input. Stopping a held direction uses
// the opposite key; reversing it is not realizable under this contract.
inline std::optional<ExtraKeys> PlanInput(int direction,PhysicalKeys physical)
{
    if(direction < -1 || direction > 1)return {};
    if(direction<0)
    {
        if(physical.right)return {};
        return ExtraKeys{!physical.left,false};
    }
    if(direction>0)
    {
        if(physical.left)return {};
        return ExtraKeys{false,!physical.right};
    }
    return ExtraKeys{physical.right && !physical.left,physical.left && !physical.right};
}
} // namespace malinka::avoid
