#pragma once

#include <cstdint>
#include <cstddef>

namespace avoidfreeze::proxy
{

// Measured BestClient 3.1.1 (DDNet.exe AMD64) offsets
inline constexpr uint32_t GAMECLIENT_VTABLE_RVA = 19904440;
inline constexpr size_t GAMECLIENT_COLLISION = 9027024;
inline constexpr size_t GAMECLIENT_LOCAL_IDS = 10988508;
inline constexpr size_t GAMECLIENT_CLIENT_DATA = 9084832;
inline constexpr size_t CLIENT_DATA_SIZE = 14784;
inline constexpr size_t CLIENT_DATA_PREDICTED_CORE = 120;

inline constexpr size_t VTABLE_PREPARE_INPUT_FOR_SEND = 21;

inline constexpr size_t COLLISION_WIDTH = 16;
inline constexpr size_t COLLISION_HEIGHT = 20;
inline constexpr size_t COLLISION_TILES = 24;
inline constexpr size_t COLLISION_FRONT = 48;

inline constexpr size_t CORE_POSITION = 16;
inline constexpr size_t CORE_VELOCITY = 24;
inline constexpr size_t CORE_DIRECTION = 216;
inline constexpr size_t CORE_SUPER = 291;
inline constexpr size_t CORE_INVINCIBLE = 292;
inline constexpr size_t CORE_IN_FREEZE = 304;
inline constexpr size_t CORE_DEEP_FROZEN = 305;
inline constexpr size_t CORE_LIVE_FROZEN = 306;

bool StartHookThread();
void StopHookThread();

bool IsAvoidFreezeEnabled();
void SetAvoidFreezeEnabled(bool enabled);

} // namespace avoidfreeze::proxy
