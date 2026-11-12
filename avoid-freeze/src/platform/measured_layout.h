#pragma once

#include <cstddef>
#include <cstdint>

namespace avoidfreeze::win32::measured
{
// Compiler-derived candidates for the exact public BestClient 3.1.1 image
// SHA-256 below. Member offsets are object-relative bytes, never module RVAs.
// Provenance: MSVC x64 layout acquisition 43010774923 and the partial,
// observe-only live-reader run 43034353037. The complete layout is not claimed
// to be runtime validated; every accessed path is separately guarded.
inline constexpr char PUBLIC_EXE_SHA256[] =
    "50430c5f1081863b956fd5f0d8e1c6b88d1aa27f79e199c4053e86a5b632902f";
inline constexpr std::uint64_t PUBLIC_EXE_SIZE = 26206720;
inline constexpr std::uint32_t PUBLIC_EXE_TIMESTAMP = 1794305545;
inline constexpr std::uint32_t PUBLIC_EXE_IMAGE_SIZE = 50638848;
inline constexpr std::uint32_t GAMECLIENT_VTABLE_RVA = 19904440;
inline constexpr std::uint32_t RTTI_LOCATOR_RVA = 24123696;
inline constexpr std::uint32_t RTTI_TYPE_DESCRIPTOR_RVA = 25441720;
inline constexpr std::uint32_t GAMECLIENT_VTABLE_FILE_OFFSET = 19898296;
inline constexpr std::uint32_t GAMECLIENT_RTTI_FIRST_FUNCTIONS[8] = {
    1578736, 5743856, 5747008, 5819088, 5766976, 5803232, 5803168, 5772320};

inline constexpr std::size_t GAMECLIENT_SIZE = 11064576;
inline constexpr std::size_t GAMECLIENT_CONTROLS = 8525880;
inline constexpr std::size_t GAMECLIENT_CLIENT = 9026824;
inline constexpr std::size_t GAMECLIENT_CONFIG = 9026848;
inline constexpr std::size_t GAMECLIENT_COLLISION = 9027024;
inline constexpr std::size_t GAMECLIENT_SNAP = 9030736;
inline constexpr std::size_t GAMECLIENT_LOCAL_IDS = 10988508;
inline constexpr std::size_t GAMECLIENT_PREDICTED_WORLD = 10992424;
inline constexpr std::size_t GAMECLIENT_TUNING_LIST = 11013512;
inline constexpr std::size_t GAMECLIENT_CLIENT_DATA = 9084832;
inline constexpr std::size_t CLIENT_DATA_SIZE = 14784;
inline constexpr std::size_t CLIENT_DATA_PREDICTED_CORE = 120;

inline constexpr std::size_t CLIENT_STATE = 7784;
inline constexpr std::size_t CLIENT_PREDICTED_TICKS = 7840;
inline constexpr std::size_t CONFIG_DUMMY = 3692;

inline constexpr std::size_t SNAP_LOCAL_CLIENT_ID = 22736;
inline constexpr std::size_t SNAP_PLAYER_COUNT = 22740;
inline constexpr std::size_t SNAP_CHARACTERS = 22792;
inline constexpr std::size_t SNAP_CHARACTER_SIZE = 240;

inline constexpr std::size_t WORLD_GAME_TICK = 1700;
inline constexpr std::size_t WORLD_FLAGS = 1704;
inline constexpr std::size_t WORLD_LOCAL_CLIENT_ID = 1752;
inline constexpr std::size_t WORLD_EGO_TILES = 1760;
inline constexpr std::size_t WORLD_ENTITY_LISTS = 1800;
inline constexpr std::size_t WORLD_CHARACTERS = 2120;
inline constexpr std::size_t WORLD_COLLISION = 3144;
inline constexpr std::size_t WORLD_TUNING = 3152;
inline constexpr std::size_t WORLD_SIZE = 3168;

inline constexpr std::size_t ENTITY_GAME_WORLD = 24;
inline constexpr std::size_t ENTITY_ID = 36;
inline constexpr std::size_t ENTITY_POSITION = 48;
inline constexpr std::size_t CHARACTER_LOCAL = 96;
inline constexpr std::size_t CHARACTER_FREEZE_TIME = 100;
inline constexpr std::size_t CHARACTER_CORE = 920;
inline constexpr std::size_t CHARACTER_SIZE = 1512;

inline constexpr std::size_t CORE_WORLD = 0;
inline constexpr std::size_t CORE_COLLISION = 8;
inline constexpr std::size_t CORE_POSITION = 16;
inline constexpr std::size_t CORE_VELOCITY = 24;
inline constexpr std::size_t CORE_HOOK_POSITION = 32;
inline constexpr std::size_t CORE_HOOK_DIRECTION = 40;
inline constexpr std::size_t CORE_HOOK_TELE_BASE = 48;
inline constexpr std::size_t CORE_HOOK_TICK = 56;
inline constexpr std::size_t CORE_HOOK_STATE = 60;
inline constexpr std::size_t CORE_ACTIVE_WEAPON = 80;
inline constexpr std::size_t CORE_NEW_HOOK = 200;
inline constexpr std::size_t CORE_JUMPED = 204;
inline constexpr std::size_t CORE_JUMPED_TOTAL = 208;
inline constexpr std::size_t CORE_JUMPS = 212;
inline constexpr std::size_t CORE_DIRECTION = 216;
inline constexpr std::size_t CORE_ANGLE = 220;
inline constexpr std::size_t CORE_INPUT = 224;
inline constexpr std::size_t CORE_EVENTS = 264;
inline constexpr std::size_t CORE_ID = 268;
inline constexpr std::size_t CORE_RESET = 272;
inline constexpr std::size_t CORE_COLLIDING = 276;
inline constexpr std::size_t CORE_LEFT_WALL = 280;
inline constexpr std::size_t CORE_SOLO = 281;
inline constexpr std::size_t CORE_JETPACK = 282;
inline constexpr std::size_t CORE_COLLISION_DISABLED = 283;
inline constexpr std::size_t CORE_ENDLESS_HOOK = 284;
inline constexpr std::size_t CORE_ENDLESS_JUMP = 285;
inline constexpr std::size_t CORE_HAMMER_DISABLED = 286;
inline constexpr std::size_t CORE_GRENADE_DISABLED = 287;
inline constexpr std::size_t CORE_LASER_DISABLED = 288;
inline constexpr std::size_t CORE_SHOTGUN_DISABLED = 289;
inline constexpr std::size_t CORE_HOOK_DISABLED = 290;
inline constexpr std::size_t CORE_SUPER = 291;
inline constexpr std::size_t CORE_INVINCIBLE = 292;
inline constexpr std::size_t CORE_TELEGUN = 293;
inline constexpr std::size_t CORE_TELEGRENADE = 294;
inline constexpr std::size_t CORE_TELELASER = 295;
inline constexpr std::size_t CORE_FREEZE_START = 296;
inline constexpr std::size_t CORE_FREEZE_END = 300;
inline constexpr std::size_t CORE_IN_FREEZE = 304;
inline constexpr std::size_t CORE_DEEP_FROZEN = 305;
inline constexpr std::size_t CORE_LIVE_FROZEN = 306;
inline constexpr std::size_t CORE_TUNING = 308;
inline constexpr std::size_t CORE_HOOKED_PLAYER = 508;
inline constexpr std::size_t CORE_SIZE = 576;
inline constexpr std::size_t TUNING_SIZE = 188;
inline constexpr std::size_t PLAYER_INPUT_SIZE = 40;

inline constexpr std::size_t COLLISION_WIDTH = 16;
inline constexpr std::size_t COLLISION_HEIGHT = 20;
inline constexpr std::size_t COLLISION_GAME_TILES = 24;
inline constexpr std::size_t COLLISION_TELE_LAYER = 32;
inline constexpr std::size_t COLLISION_SPEEDUP_LAYER = 40;
inline constexpr std::size_t COLLISION_FRONT_LAYER = 48;
inline constexpr std::size_t COLLISION_SWITCH_LAYER = 56;
inline constexpr std::size_t COLLISION_TUNE_LAYER = 64;
inline constexpr std::size_t COLLISION_DOOR_LAYER = 72;
inline constexpr std::size_t COLLISION_EXTRA_DOOR_LAYER = 80;
inline constexpr std::size_t COLLISION_SIZE = 160;

inline constexpr std::size_t CONFIG_SIZE = 26492;
inline constexpr std::size_t MAX_MAP_TILES = 262144;
inline constexpr std::uint64_t MAX_USER_ADDRESS = 0x00007fffffffffffULL;
} // namespace avoidfreeze::win32::measured
