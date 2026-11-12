#include "win32_reader.h"
#include "layout_check.h"
#include "measured_layout.h"

#include <engine/client.h>
#include <engine/shared/config.h>
#include <game/client/gameclient.h>
#include <game/client/prediction/entities/character.h>
#include <game/client/prediction/gameworld.h>
#include <game/mapitems.h>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <bcrypt.h>
#include <tlhelp32.h>

#ifdef ERROR
#undef ERROR
#endif
#ifdef ABSOLUTE
#undef ABSOLUTE
#endif
#ifdef RELATIVE
#undef RELATIVE
#endif
#ifdef IGNORE
#undef IGNORE
#endif

#include <algorithm>
#include <array>
#include <bit>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <iterator>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace avoidfreeze::win32
{
namespace
{
using Clock = std::chrono::steady_clock;
using Byte = std::byte;
constexpr std::uint64_t MAX_SCAN_BYTES = 512ULL * 1024ULL * 1024ULL;
constexpr std::uint64_t MAX_SCAN_REGIONS = 65536;
constexpr std::uint64_t MAX_SCAN_MS = 1500;
constexpr std::size_t MAX_READ_BYTES = 1024U * 1024U;
constexpr std::size_t READ_CHUNK_BYTES = 64U * 1024U;

class Handle final
{
    HANDLE m_Value{};

public:
    Handle() = default;
    explicit Handle(HANDLE value) : m_Value(value) {}
    ~Handle() { Reset(); }
    Handle(const Handle &) = delete;
    Handle &operator=(const Handle &) = delete;
    Handle(Handle &&other) noexcept : m_Value(std::exchange(other.m_Value, nullptr)) {}
    Handle &operator=(Handle &&other) noexcept
    {
        if(this != &other)
        {
            Reset();
            m_Value = std::exchange(other.m_Value, nullptr);
        }
        return *this;
    }
    void Reset(HANDLE value = nullptr) noexcept
    {
        if(m_Value != nullptr && m_Value != INVALID_HANDLE_VALUE)
            CloseHandle(m_Value);
        m_Value = value;
    }
    HANDLE Get() const noexcept { return m_Value; }
    explicit operator bool() const noexcept { return m_Value != nullptr && m_Value != INVALID_HANDLE_VALUE; }
};

std::string WinError(const char *operation, DWORD error = GetLastError())
{
    return std::string(operation) + " failed (Win32 " + std::to_string(error) + ")";
}

bool UserRange(std::uint64_t address, std::uint64_t length) noexcept
{
    return address >= 0x10000 && address <= measured::MAX_USER_ADDRESS && length != 0 &&
           length - 1 <= measured::MAX_USER_ADDRESS - address;
}

std::uint64_t AddAddress(std::uint64_t address, std::uint64_t offset)
{
    if(offset > measured::MAX_USER_ADDRESS - address)
        throw std::runtime_error("remote address arithmetic overflow");
    const std::uint64_t result = address + offset;
    if(result > measured::MAX_USER_ADDRESS)
        throw std::runtime_error("remote address exceeds the measured user range");
    return result;
}

std::uint64_t FileTimeValue(const FILETIME &time) noexcept
{
    return (static_cast<std::uint64_t>(time.dwHighDateTime) << 32) | time.dwLowDateTime;
}

std::uint64_t CreationTime(HANDLE process)
{
    FILETIME creation{}, exit{}, kernel{}, user{};
    if(!GetProcessTimes(process, &creation, &exit, &kernel, &user))
        throw std::runtime_error(WinError("GetProcessTimes"));
    return FileTimeValue(creation);
}

bool SamePath(std::wstring_view first, std::wstring_view second)
{
    if(first.size() > 32768 || second.size() > 32768)
        return false;
    return CompareStringOrdinal(first.data(), static_cast<int>(first.size()),
                                second.data(), static_cast<int>(second.size()), TRUE) == CSTR_EQUAL;
}

std::string Sha256File(const std::wstring &path)
{
    Handle file(CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_DELETE, nullptr,
                            OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, nullptr));
    if(!file)
        throw std::runtime_error(WinError("open executable read-only"));
    if(GetFileType(file.Get()) != FILE_TYPE_DISK)
        throw std::runtime_error("target image is not a regular disk file");

    BY_HANDLE_FILE_INFORMATION before{}, after{};
    if(!GetFileInformationByHandle(file.Get(), &before) || (before.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0)
        throw std::runtime_error(WinError("inspect executable file"));
    const std::uint64_t size = (static_cast<std::uint64_t>(before.nFileSizeHigh) << 32) | before.nFileSizeLow;
    if(size != measured::PUBLIC_EXE_SIZE)
        throw std::runtime_error("executable size does not match the audited public image profile");

    BCRYPT_ALG_HANDLE algorithm{};
    if(BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0) < 0)
        throw std::runtime_error("BCrypt SHA-256 provider could not be opened");
    struct AlgorithmGuard
    {
        BCRYPT_ALG_HANDLE value;
        ~AlgorithmGuard() { if(value != nullptr) BCryptCloseAlgorithmProvider(value, 0); }
    } algorithm_guard{algorithm};

    DWORD object_size{}, hash_size{}, used{};
    if(BCryptGetProperty(algorithm, BCRYPT_OBJECT_LENGTH, reinterpret_cast<PUCHAR>(&object_size), sizeof(object_size), &used, 0) < 0 ||
       used != sizeof(object_size) || BCryptGetProperty(algorithm, BCRYPT_HASH_LENGTH, reinterpret_cast<PUCHAR>(&hash_size), sizeof(hash_size), &used, 0) < 0 ||
       used != sizeof(hash_size) || object_size == 0 || object_size > 1024U * 1024U || hash_size != 32)
        throw std::runtime_error("unexpected BCrypt SHA-256 properties");

    std::vector<UCHAR> object(object_size);
    BCRYPT_HASH_HANDLE hash{};
    if(BCryptCreateHash(algorithm, &hash, object.data(), object_size, nullptr, 0, 0) < 0)
        throw std::runtime_error("BCrypt SHA-256 hash initialization failed");
    struct HashGuard
    {
        BCRYPT_HASH_HANDLE value;
        ~HashGuard() { if(value != nullptr) BCryptDestroyHash(value); }
    } hash_guard{hash};

    std::array<UCHAR, 64U * 1024U> buffer{};
    std::uint64_t total{};
    while(total < size)
    {
        const DWORD requested = static_cast<DWORD>(std::min<std::uint64_t>(buffer.size(), size - total));
        DWORD read{};
        if(!ReadFile(file.Get(), buffer.data(), requested, &read, nullptr) || read != requested)
            throw std::runtime_error("executable file read was failed or partial");
        if(BCryptHashData(hash, buffer.data(), read, 0) < 0)
            throw std::runtime_error("BCrypt SHA-256 update failed");
        total += read;
    }

    std::array<UCHAR, 32> digest{};
    if(BCryptFinishHash(hash, digest.data(), static_cast<ULONG>(digest.size()), 0) < 0 ||
       !GetFileInformationByHandle(file.Get(), &after) ||
       before.nFileSizeHigh != after.nFileSizeHigh || before.nFileSizeLow != after.nFileSizeLow ||
       before.nFileIndexHigh != after.nFileIndexHigh || before.nFileIndexLow != after.nFileIndexLow ||
       CompareFileTime(&before.ftLastWriteTime, &after.ftLastWriteTime) != 0)
        throw std::runtime_error("executable changed during read-only identity check");

    constexpr char HEX[] = "0123456789abcdef";
    std::string output;
    output.reserve(64);
    for(const UCHAR value : digest)
    {
        output.push_back(HEX[value >> 4]);
        output.push_back(HEX[value & 15]);
    }
    return output;
}

std::vector<Byte> ReadRemote(HANDLE process, std::uint64_t address, std::size_t length)
{
    if(length == 0 || length > MAX_READ_BYTES || !UserRange(address, length))
        throw std::runtime_error("remote read is outside its explicit size/address bound");
    std::vector<Byte> output(length);
    SIZE_T read{};
    if(!ReadProcessMemory(process, reinterpret_cast<LPCVOID>(static_cast<std::uintptr_t>(address)),
                          output.data(), output.size(), &read) || read != output.size())
        throw std::runtime_error(WinError("ReadProcessMemory (read-only)"));
    return output;
}

std::uint64_t ReadU64(HANDLE process, std::uint64_t address)
{
    const auto bytes = ReadRemote(process, address, sizeof(std::uint64_t));
    std::uint64_t value{};
    std::memcpy(&value, bytes.data(), sizeof(value));
    return value;
}

std::uint32_t ReadU32(HANDLE process, std::uint64_t address)
{
    const auto bytes = ReadRemote(process, address, sizeof(std::uint32_t));
    std::uint32_t value{};
    std::memcpy(&value, bytes.data(), sizeof(value));
    return value;
}

std::int32_t ReadI32(HANDLE process, std::uint64_t address)
{
    return std::bit_cast<std::int32_t>(ReadU32(process, address));
}

bool ReadBool(HANDLE process, std::uint64_t address)
{
    const auto bytes = ReadRemote(process, address, 1);
    const auto value = std::to_integer<unsigned char>(bytes[0]);
    if(value > 1)
        throw std::runtime_error("remote bool is not canonical");
    return value != 0;
}

std::uint32_t ObjectU32(std::span<const Byte> bytes, std::size_t offset)
{
    if(offset > bytes.size() || bytes.size() - offset < sizeof(std::uint32_t))
        throw std::runtime_error("measured object field exceeds its copied object");
    std::uint32_t value{};
    std::memcpy(&value, bytes.data() + offset, sizeof(value));
    return value;
}

std::int32_t ObjectI32(std::span<const Byte> bytes, std::size_t offset)
{
    return std::bit_cast<std::int32_t>(ObjectU32(bytes, offset));
}

std::uint64_t ObjectU64(std::span<const Byte> bytes, std::size_t offset)
{
    if(offset > bytes.size() || bytes.size() - offset < sizeof(std::uint64_t))
        throw std::runtime_error("measured pointer field exceeds its copied object");
    std::uint64_t value{};
    std::memcpy(&value, bytes.data() + offset, sizeof(value));
    return value;
}

bool ObjectBool(std::span<const Byte> bytes, std::size_t offset)
{
    if(offset >= bytes.size())
        throw std::runtime_error("measured bool field exceeds its copied object");
    const auto value = std::to_integer<unsigned char>(bytes[offset]);
    if(value > 1)
        throw std::runtime_error("copied object contains a non-canonical bool");
    return value != 0;
}

float ObjectFloat(std::span<const Byte> bytes, std::size_t offset)
{
    const float value = std::bit_cast<float>(ObjectU32(bytes, offset));
    if(!std::isfinite(value) || std::abs(value) > 10000000.0f)
        throw std::runtime_error("copied object contains a non-finite or excessive float");
    return value;
}

vec2 ObjectVector(std::span<const Byte> bytes, std::size_t offset)
{
    return {ObjectFloat(bytes, offset), ObjectFloat(bytes, offset + sizeof(float))};
}

bool ReadableProtection(DWORD protection) noexcept
{
    const DWORD basic = protection & 0xffU;
    const bool readable = basic == PAGE_READONLY || basic == PAGE_READWRITE || basic == PAGE_WRITECOPY ||
                          basic == PAGE_EXECUTE_READ || basic == PAGE_EXECUTE_READWRITE || basic == PAGE_EXECUTE_WRITECOPY;
    return readable && (protection & (PAGE_GUARD | PAGE_NOACCESS)) == 0;
}

struct ModuleInfo
{
    std::uint64_t base{};
    std::uint64_t size{};
};

ModuleInfo FindMainModule(DWORD process_id, std::wstring_view expected_path)
{
    Handle modules(CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, process_id));
    if(!modules)
        throw std::runtime_error(WinError("CreateToolhelp32Snapshot"));
    MODULEENTRY32W entry{};
    entry.dwSize = sizeof(entry);
    if(!Module32FirstW(modules.Get(), &entry))
        throw std::runtime_error(WinError("Module32FirstW"));
    for(unsigned int count = 0; count < 1024; ++count)
    {
        if(SamePath(expected_path, entry.szExePath))
        {
            const auto base = reinterpret_cast<std::uintptr_t>(entry.modBaseAddr);
            const auto size = static_cast<std::uint64_t>(entry.modBaseSize);
            if(!UserRange(base, size))
                throw std::runtime_error("main executable module range is invalid");
            return {base, size};
        }
        if(!Module32NextW(modules.Get(), &entry))
        {
            if(GetLastError() != ERROR_NO_MORE_FILES)
                throw std::runtime_error(WinError("Module32NextW"));
            break;
        }
    }
    throw std::runtime_error("loaded main module path does not match the process image path");
}

void ValidateLoadedHeaders(HANDLE process, const ModuleInfo &module)
{
    const auto dos_bytes = ReadRemote(process, module.base, sizeof(IMAGE_DOS_HEADER));
    IMAGE_DOS_HEADER dos{};
    std::memcpy(&dos, dos_bytes.data(), sizeof(dos));
    if(dos.e_magic != IMAGE_DOS_SIGNATURE || dos.e_lfanew < static_cast<LONG>(sizeof(IMAGE_DOS_HEADER)) || dos.e_lfanew > 1024 * 1024)
        throw std::runtime_error("loaded PE DOS header differs from the audited image");

    const std::uint64_t nt_address = AddAddress(module.base, static_cast<std::uint32_t>(dos.e_lfanew));
    const auto nt_bytes = ReadRemote(process, nt_address, sizeof(IMAGE_NT_HEADERS64));
    IMAGE_NT_HEADERS64 nt{};
    std::memcpy(&nt, nt_bytes.data(), sizeof(nt));
    if(nt.Signature != IMAGE_NT_SIGNATURE || nt.FileHeader.Machine != IMAGE_FILE_MACHINE_AMD64 ||
       nt.OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR64_MAGIC ||
       nt.FileHeader.TimeDateStamp != measured::PUBLIC_EXE_TIMESTAMP ||
       nt.OptionalHeader.SizeOfImage != measured::PUBLIC_EXE_IMAGE_SIZE ||
       module.size != measured::PUBLIC_EXE_IMAGE_SIZE)
        throw std::runtime_error("loaded PE headers do not match the audited AMD64 image profile");
}

void ValidateLoadedVtable(HANDLE process, const ModuleInfo &module)
{
    std::array<std::uint64_t, 9> entries{};
    const auto bytes = ReadRemote(process, AddAddress(module.base, measured::GAMECLIENT_VTABLE_RVA - 8), sizeof(entries));
    std::memcpy(entries.data(), bytes.data(), sizeof(entries));
    if(entries[0] != AddAddress(module.base, measured::RTTI_LOCATOR_RVA))
        throw std::runtime_error("loaded GameClient RTTI locator fingerprint mismatch");
    for(std::size_t index = 0; index < std::size(measured::GAMECLIENT_RTTI_FIRST_FUNCTIONS); ++index)
    {
        if(entries[index + 1] != AddAddress(module.base, measured::GAMECLIENT_RTTI_FIRST_FUNCTIONS[index]))
            throw std::runtime_error("loaded GameClient vtable fingerprint mismatch");
    }
}

std::uint64_t FindUniqueRoot(HANDLE process, std::uint64_t vtable_pointer)
{
    if(!UserRange(vtable_pointer, sizeof(std::uint64_t)))
        throw std::runtime_error("audited GameClient vtable pointer is outside the target range");
    const auto start = Clock::now();
    std::uint64_t address = 0x10000;
    std::uint64_t attempted{};
    std::uint64_t regions{};
    std::vector<std::uint64_t> candidates;
    std::array<Byte, READ_CHUNK_BYTES> buffer{};

    while(address <= measured::MAX_USER_ADDRESS)
    {
        if(WaitForSingleObject(process, 0) != WAIT_TIMEOUT)
            throw std::runtime_error("target exited during root scan");
        if(regions >= MAX_SCAN_REGIONS || attempted >= MAX_SCAN_BYTES ||
           static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - start).count()) >= MAX_SCAN_MS)
            throw std::runtime_error("bounded root scan did not complete; uniqueness is unknown");

        MEMORY_BASIC_INFORMATION info{};
        const SIZE_T queried = VirtualQueryEx(process, reinterpret_cast<LPCVOID>(static_cast<std::uintptr_t>(address)), &info, sizeof(info));
        if(queried != sizeof(info))
        {
            if(GetLastError() == ERROR_INVALID_PARAMETER)
                break;
            throw std::runtime_error(WinError("VirtualQueryEx"));
        }
        ++regions;
        const auto region_base = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(info.BaseAddress));
        const auto region_size = static_cast<std::uint64_t>(info.RegionSize);
        if(region_size == 0 || region_base > address || !UserRange(region_base, region_size) ||
           region_base + region_size <= address)
            throw std::runtime_error("VirtualQueryEx returned an inconsistent memory region");
        const auto region_end = region_base + region_size;

        if(info.State == MEM_COMMIT && info.Type == MEM_PRIVATE && ReadableProtection(info.Protect))
        {
            std::uint64_t current = std::max(address, region_base);
            const std::uint64_t misalignment = current % alignof(std::uint64_t);
            if(misalignment != 0)
                current += alignof(std::uint64_t) - misalignment;
            while(current <= region_end && region_end - current >= sizeof(std::uint64_t))
            {
                if(WaitForSingleObject(process, 0) != WAIT_TIMEOUT)
                    throw std::runtime_error("target exited during root scan");
                const auto elapsed = static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - start).count());
                if(attempted >= MAX_SCAN_BYTES || elapsed >= MAX_SCAN_MS)
                    throw std::runtime_error("bounded root scan did not complete; uniqueness is unknown");
                const std::uint64_t remaining_region = region_end - current;
                const std::uint64_t remaining_budget = MAX_SCAN_BYTES - attempted;
                std::uint64_t request = std::min<std::uint64_t>({remaining_region, buffer.size(), remaining_budget});
                request -= request % alignof(std::uint64_t);
                if(request < sizeof(std::uint64_t))
                    break;
                SIZE_T read{};
                if(!ReadProcessMemory(process, reinterpret_cast<LPCVOID>(static_cast<std::uintptr_t>(current)),
                                     buffer.data(), static_cast<SIZE_T>(request), &read) || read != request)
                    throw std::runtime_error(WinError("ReadProcessMemory during root scan"));
                attempted += request;
                for(std::size_t offset = 0; offset + sizeof(std::uint64_t) <= static_cast<std::size_t>(request); offset += alignof(std::uint64_t))
                {
                    std::uint64_t value{};
                    std::memcpy(&value, buffer.data() + offset, sizeof(value));
                    if(value == vtable_pointer)
                    {
                        const std::uint64_t candidate = current + offset;
                        if(UserRange(candidate, measured::GAMECLIENT_SIZE))
                        {
                            candidates.push_back(candidate);
                            if(candidates.size() > 1)
                                throw std::runtime_error("multiple GameClient root candidates; target is ambiguous");
                        }
                    }
                }
                current += request;
            }
        }
        address = region_end;
    }

    if(candidates.size() != 1)
        throw std::runtime_error("complete private-memory scan did not find exactly one GameClient root");
    if(WaitForSingleObject(process, 0) != WAIT_TIMEOUT)
        throw std::runtime_error("target exited before root validation completed");
    return candidates.front();
}

void RequirePointer(HANDLE process, std::uint64_t pointer, std::size_t length, const char *label)
{
    if(!UserRange(pointer, length))
        throw std::runtime_error(std::string(label) + " pointer/range is invalid");
    (void)ReadRemote(process, pointer, 1);
}

bool EqualVectors(const std::vector<Byte> &first, const std::vector<Byte> &second) noexcept
{
    return first.size() == second.size() && std::equal(first.begin(), first.end(), second.begin());
}

bool EqualCoreField(std::span<const Byte> first, std::span<const Byte> second, std::size_t offset, std::size_t length)
{
    if(offset > first.size() || offset > second.size() || length > first.size() - offset || length > second.size() - offset)
        throw std::runtime_error("predicted core field exceeds its measured object");
    return std::equal(first.begin() + static_cast<std::ptrdiff_t>(offset),
                      first.begin() + static_cast<std::ptrdiff_t>(offset + length),
                      second.begin() + static_cast<std::ptrdiff_t>(offset));
}

bool CoreCopiesAgree(std::span<const Byte> first, std::span<const Byte> second)
{
    constexpr std::array<std::pair<std::size_t, std::size_t>, 22> fields{{
        {measured::CORE_POSITION, 8}, {measured::CORE_VELOCITY, 8},
        {measured::CORE_HOOK_POSITION, 8}, {measured::CORE_HOOK_DIRECTION, 8},
        {measured::CORE_HOOK_TELE_BASE, 8}, {measured::CORE_HOOK_TICK, 4},
        {measured::CORE_HOOK_STATE, 4}, {measured::CORE_JUMPED, 4},
        {measured::CORE_JUMPED_TOTAL, 4}, {measured::CORE_JUMPS, 4},
        {measured::CORE_DIRECTION, 4}, {measured::CORE_ANGLE, 4},
        {measured::CORE_INPUT, measured::PLAYER_INPUT_SIZE}, {measured::CORE_ID, 4},
        {measured::CORE_SOLO, 1}, {measured::CORE_ENDLESS_HOOK, 1},
        {measured::CORE_ENDLESS_JUMP, 1}, {measured::CORE_SUPER, 1},
        {measured::CORE_INVINCIBLE, 1}, {measured::CORE_IN_FREEZE, 1},
        {measured::CORE_DEEP_FROZEN, 1}, {measured::CORE_LIVE_FROZEN, 1}}};
    for(const auto &[offset, length] : fields)
        if(!EqualCoreField(first, second, offset, length))
            return false;
    return EqualCoreField(first, second, measured::CORE_TUNING, measured::TUNING_SIZE);
}

void ParseInput(std::span<const Byte> core, CharacterState &player)
{
    const std::size_t at = measured::CORE_INPUT;
    player.input.m_Direction = ObjectI32(core, at);
    player.input.m_TargetX = ObjectI32(core, at + 4);
    player.input.m_TargetY = ObjectI32(core, at + 8);
    player.input.m_Jump = ObjectI32(core, at + 12);
    player.input.m_Fire = ObjectI32(core, at + 16);
    player.input.m_Hook = ObjectI32(core, at + 20);
    player.input.m_PlayerFlags = ObjectI32(core, at + 24);
    player.input.m_WantedWeapon = ObjectI32(core, at + 28);
    player.input.m_NextWeapon = ObjectI32(core, at + 32);
    player.input.m_PrevWeapon = ObjectI32(core, at + 36);
}

void DecodePlayer(std::span<const Byte> character, int client_id, int tick, CharacterState &player)
{
    const auto core = character.subspan(measured::CHARACTER_CORE, measured::CORE_SIZE);
    player.client_id = client_id;
    player.tick = tick;
    player.freeze_time = ObjectI32(character, measured::CHARACTER_FREEZE_TIME);
    player.position = ObjectVector(core, measured::CORE_POSITION);
    player.velocity = ObjectVector(core, measured::CORE_VELOCITY);
    player.hook_position = ObjectVector(core, measured::CORE_HOOK_POSITION);
    player.hook_direction = ObjectVector(core, measured::CORE_HOOK_DIRECTION);
    player.hook_tele_base = ObjectVector(core, measured::CORE_HOOK_TELE_BASE);
    player.hook_tick = ObjectI32(core, measured::CORE_HOOK_TICK);
    player.hook_state = ObjectI32(core, measured::CORE_HOOK_STATE);
    player.active_weapon = ObjectI32(core, measured::CORE_ACTIVE_WEAPON);
    player.new_hook = ObjectBool(core, measured::CORE_NEW_HOOK);
    player.jumped = ObjectI32(core, measured::CORE_JUMPED);
    player.jumped_total = ObjectI32(core, measured::CORE_JUMPED_TOTAL);
    player.jumps = ObjectI32(core, measured::CORE_JUMPS);
    player.direction = ObjectI32(core, measured::CORE_DIRECTION);
    player.angle = ObjectI32(core, measured::CORE_ANGLE);
    ParseInput(core, player);
    player.triggered_events = ObjectI32(core, measured::CORE_EVENTS);
    player.reset = ObjectBool(core, measured::CORE_RESET);
    player.colliding = ObjectI32(core, measured::CORE_COLLIDING);
    player.left_wall = ObjectBool(core, measured::CORE_LEFT_WALL);
    player.solo = ObjectBool(core, measured::CORE_SOLO);
    player.jetpack = ObjectBool(core, measured::CORE_JETPACK);
    player.collision_disabled = ObjectBool(core, measured::CORE_COLLISION_DISABLED);
    player.endless_hook = ObjectBool(core, measured::CORE_ENDLESS_HOOK);
    player.endless_jump = ObjectBool(core, measured::CORE_ENDLESS_JUMP);
    player.hammer_hit_disabled = ObjectBool(core, measured::CORE_HAMMER_DISABLED);
    player.grenade_hit_disabled = ObjectBool(core, measured::CORE_GRENADE_DISABLED);
    player.laser_hit_disabled = ObjectBool(core, measured::CORE_LASER_DISABLED);
    player.shotgun_hit_disabled = ObjectBool(core, measured::CORE_SHOTGUN_DISABLED);
    player.hook_hit_disabled = ObjectBool(core, measured::CORE_HOOK_DISABLED);
    player.super = ObjectBool(core, measured::CORE_SUPER);
    player.invincible = ObjectBool(core, measured::CORE_INVINCIBLE);
    player.has_telegun = ObjectBool(core, measured::CORE_TELEGUN);
    player.has_telegrenade = ObjectBool(core, measured::CORE_TELEGRENADE);
    player.has_telelaser = ObjectBool(core, measured::CORE_TELELASER);
    player.freeze_start = ObjectI32(core, measured::CORE_FREEZE_START);
    player.freeze_end = ObjectI32(core, measured::CORE_FREEZE_END);
    player.in_freeze = ObjectBool(core, measured::CORE_IN_FREEZE);
    player.deep_frozen = ObjectBool(core, measured::CORE_DEEP_FROZEN);
    player.live_frozen = ObjectBool(core, measured::CORE_LIVE_FROZEN);

    constexpr std::size_t TUNING_COUNT = measured::TUNING_SIZE / sizeof(std::int32_t);
    static_assert(measured::TUNING_SIZE % sizeof(std::int32_t) == 0);
    for(std::size_t index = 0; index < TUNING_COUNT; ++index)
        player.tuning.NetworkArray()[index] = ObjectI32(core, measured::CORE_TUNING + index * sizeof(std::int32_t));
}

TileMapSnapshot DecodeMap(int width, int height, std::span<const Byte> game_bytes, std::span<const Byte> front_bytes)
{
    static_assert(sizeof(CTile) == 4);
    const auto cells = static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
    if(game_bytes.size() != cells * sizeof(CTile) || (!front_bytes.empty() && front_bytes.size() != cells * sizeof(CTile)))
        throw std::runtime_error("tile array length does not match the measured map geometry");
    TileMapSnapshot map;
    map.width = width;
    map.height = height;
    map.game.resize(cells);
    std::memcpy(map.game.data(), game_bytes.data(), game_bytes.size());
    if(!front_bytes.empty())
    {
        map.front.resize(cells);
        std::memcpy(map.front.data(), front_bytes.data(), front_bytes.size());
    }
    ValidateStaticMap(map);
    return map;
}

} // namespace

struct Win32Reader::Impl
{
    std::uint32_t process_id{};
    std::uint64_t creation_time{};
    ModuleInfo module;
    std::uint64_t root{};
    Handle process;

    explicit Impl(std::uint32_t pid) : process_id(pid)
    {
        static_assert(sizeof(void *) == 8);
        static_assert(std::endian::native == std::endian::little);
        ValidateMeasuredLayout();
        if(process_id == 0)
            throw std::invalid_argument("process ID must be a positive decimal number");

        constexpr DWORD READ_ONLY_RIGHTS = PROCESS_QUERY_INFORMATION | PROCESS_QUERY_LIMITED_INFORMATION | PROCESS_VM_READ | SYNCHRONIZE;
        process.Reset(OpenProcess(READ_ONLY_RIGHTS, FALSE, process_id));
        if(!process)
            throw std::runtime_error(WinError("OpenProcess with read-only rights"));
        if(WaitForSingleObject(process.Get(), 0) != WAIT_TIMEOUT)
            throw std::runtime_error("target process is not running");

        USHORT process_machine{}, native_machine{};
        if(!IsWow64Process2(process.Get(), &process_machine, &native_machine) ||
           process_machine != IMAGE_FILE_MACHINE_UNKNOWN || native_machine != IMAGE_FILE_MACHINE_AMD64)
            throw std::runtime_error("target is not a native AMD64 process");
        creation_time = CreationTime(process.Get());

        std::array<wchar_t, 32768> path{};
        DWORD path_length = static_cast<DWORD>(path.size());
        if(!QueryFullProcessImageNameW(process.Get(), 0, path.data(), &path_length) || path_length == 0 || path_length >= path.size())
            throw std::runtime_error(WinError("QueryFullProcessImageNameW"));
        const std::wstring image_path(path.data(), path_length);
        if(Sha256File(image_path) != measured::PUBLIC_EXE_SHA256)
            throw std::runtime_error("target executable SHA-256 is not the exact audited public BestClient image");

        module = FindMainModule(process_id, image_path);
        ValidateLoadedHeaders(process.Get(), module);
        ValidateLoadedVtable(process.Get(), module);
        root = FindUniqueRoot(process.Get(), AddAddress(module.base, measured::GAMECLIENT_VTABLE_RVA));
        if(ReadU64(process.Get(), root) != AddAddress(module.base, measured::GAMECLIENT_VTABLE_RVA))
            throw std::runtime_error("unique root candidate failed its object-vtable check");
        CheckIdentity();
    }

    void CheckIdentity() const
    {
        if(WaitForSingleObject(process.Get(), 0) != WAIT_TIMEOUT || CreationTime(process.Get()) != creation_time)
            throw std::runtime_error("target exited or PID identity changed");
        if(ReadU64(process.Get(), root) != AddAddress(module.base, measured::GAMECLIENT_VTABLE_RVA))
            throw std::runtime_error("GameClient root vtable changed");
        ValidateLoadedVtable(process.Get(), module);
    }

    SnapshotFrame Capture()
    {
        const auto start = Clock::now();
        CheckIdentity();
        RequirePointer(process.Get(), root, measured::GAMECLIENT_SIZE, "GameClient root");
        const auto client = ReadU64(process.Get(), AddAddress(root, measured::GAMECLIENT_CLIENT));
        const auto config = ReadU64(process.Get(), AddAddress(root, measured::GAMECLIENT_CONFIG));
        RequirePointer(process.Get(), client, measured::CLIENT_PREDICTED_TICKS + 8, "client interface");
        RequirePointer(process.Get(), config, measured::CONFIG_SIZE, "configuration");
        if(ReadI32(process.Get(), AddAddress(client, measured::CLIENT_STATE)) != IClient::STATE_ONLINE)
            throw std::runtime_error("client is not online");

        const int dummy = ReadI32(process.Get(), AddAddress(config, measured::CONFIG_DUMMY));
        if(dummy < 0 || dummy >= NUM_DUMMIES)
            throw std::runtime_error("selected dummy index is outside the pinned source range");
        const auto dummy_slot = static_cast<std::uint64_t>(dummy);
        const int predicted_tick = ReadI32(process.Get(), AddAddress(client, measured::CLIENT_PREDICTED_TICKS + dummy_slot * sizeof(std::int32_t)));
        const auto world = AddAddress(root, measured::GAMECLIENT_PREDICTED_WORLD);
        const auto collision = AddAddress(root, measured::GAMECLIENT_COLLISION);
        const auto snap = AddAddress(root, measured::GAMECLIENT_SNAP);
        RequirePointer(process.Get(), world, measured::WORLD_SIZE, "predicted world");

        const int world_tick = ReadI32(process.Get(), AddAddress(world, measured::WORLD_GAME_TICK));
        const int world_client_id = ReadI32(process.Get(), AddAddress(world, measured::WORLD_LOCAL_CLIENT_ID));
        const int snap_client_id = ReadI32(process.Get(), AddAddress(snap, measured::SNAP_LOCAL_CLIENT_ID));
        const int player_count = ReadI32(process.Get(), AddAddress(snap, measured::SNAP_PLAYER_COUNT));
        if(predicted_tick < 0 || world_tick != predicted_tick || player_count != 1 ||
           snap_client_id < 0 || snap_client_id >= MAX_CLIENTS || world_client_id != snap_client_id)
            throw std::runtime_error("snapshot/world/local ID or prediction tick is unsupported");
        const int client_id = snap_client_id;
        const auto local_id_address = AddAddress(root, measured::GAMECLIENT_LOCAL_IDS + dummy_slot * sizeof(std::int32_t));
        if(ReadI32(process.Get(), local_id_address) != client_id)
            throw std::runtime_error("dummy/local client ID disagreement");
        if(!ReadBool(process.Get(), AddAddress(snap, measured::SNAP_CHARACTERS +
                                              static_cast<std::uint64_t>(client_id) * measured::SNAP_CHARACTER_SIZE)))
            throw std::runtime_error("snapshot has no active local character");

        const auto world_characters = ReadRemote(process.Get(), AddAddress(world, measured::WORLD_CHARACTERS), MAX_CLIENTS * sizeof(std::uint64_t));
        std::uint64_t character_pointer{};
        for(int id = 0; id < MAX_CLIENTS; ++id)
        {
            const auto value = ObjectU64(world_characters, static_cast<std::size_t>(id) * sizeof(std::uint64_t));
            if(id == client_id)
                character_pointer = value;
            else if(value != 0)
                throw std::runtime_error("another predicted character is present");
        }
        RequirePointer(process.Get(), character_pointer, measured::CHARACTER_SIZE, "local character");

        const auto entity_lists = ReadRemote(process.Get(), AddAddress(world, measured::WORLD_ENTITY_LISTS),
                                             CGameWorld::NUM_ENTTYPES * sizeof(std::uint64_t));
        for(int type = 0; type < CGameWorld::NUM_ENTTYPES; ++type)
        {
            const auto value = ObjectU64(entity_lists, static_cast<std::size_t>(type) * sizeof(std::uint64_t));
            const auto expected = type == CGameWorld::ENTTYPE_CHARACTER ? character_pointer : 0;
            if(value != expected)
                throw std::runtime_error("non-character predicted entity is present");
        }
        if(ReadU64(process.Get(), AddAddress(world, measured::WORLD_COLLISION)) != collision)
            throw std::runtime_error("predicted world/collision backlink mismatch");
        if(ReadU64(process.Get(), AddAddress(world, measured::WORLD_EGO_TILES)) != 0)
            throw std::runtime_error("ego-tile prediction is not supported by this standalone milestone");
        const auto tuning_pointer = ReadU64(process.Get(), AddAddress(world, measured::WORLD_TUNING));
        const auto expected_tuning = AddAddress(root, measured::GAMECLIENT_TUNING_LIST);
        if(tuning_pointer != expected_tuning)
            throw std::runtime_error("effective tuning pointer differs from the measured public layout");

        const auto collision_bytes = ReadRemote(process.Get(), collision, measured::COLLISION_SIZE);
        const int width = ObjectI32(collision_bytes, measured::COLLISION_WIDTH);
        const int height = ObjectI32(collision_bytes, measured::COLLISION_HEIGHT);
        if(width <= 0 || height <= 0 ||
           static_cast<std::uint64_t>(width) > measured::MAX_MAP_TILES / static_cast<std::uint64_t>(height))
            throw std::runtime_error("collision map dimensions exceed the supported static-map budget");
        const auto cell_count = static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
        const std::size_t tile_bytes = cell_count * sizeof(CTile);
        const auto game_tiles = ObjectU64(collision_bytes, measured::COLLISION_GAME_TILES);
        const auto front_tiles = ObjectU64(collision_bytes, measured::COLLISION_FRONT_LAYER);
        for(const auto offset : {measured::COLLISION_TELE_LAYER, measured::COLLISION_SPEEDUP_LAYER,
                                 measured::COLLISION_SWITCH_LAYER, measured::COLLISION_TUNE_LAYER,
                                 measured::COLLISION_DOOR_LAYER, measured::COLLISION_EXTRA_DOOR_LAYER})
        {
            if(ObjectU64(collision_bytes, offset) != 0)
                throw std::runtime_error("teleport/speedup/switch/tune/door layers are unsupported");
        }
        if(!UserRange(game_tiles, tile_bytes) || (front_tiles != 0 && !UserRange(front_tiles, tile_bytes)))
            throw std::runtime_error("game/front tile array range is invalid");

        const auto flags = ReadRemote(process.Get(), AddAddress(world, measured::WORLD_FLAGS), 24);
        const bool is_ddrace = ObjectBool(flags, 0);
        const bool predict_tiles = ObjectBool(flags, 4);
        const int predict_freeze = ObjectI32(flags, 8);
        const bool predict_ddrace = ObjectBool(flags, 13);
        if(!is_ddrace || !predict_tiles || !predict_ddrace || predict_freeze <= 0 || predict_freeze > 2)
            throw std::runtime_error("client prediction flags are outside the supported DDNet mode");

        const auto character = ReadRemote(process.Get(), character_pointer, measured::CHARACTER_SIZE);
        if(ObjectU64(character, measured::ENTITY_GAME_WORLD) != world ||
           ObjectI32(character, measured::ENTITY_ID) != client_id ||
           !ObjectBool(character, measured::CHARACTER_LOCAL))
            throw std::runtime_error("character/world/local backlinks disagree");
        const auto core = std::span<const Byte>(character).subspan(measured::CHARACTER_CORE, measured::CORE_SIZE);
        if(ObjectU64(core, measured::CORE_WORLD) != world || ObjectU64(core, measured::CORE_COLLISION) != collision ||
           ObjectI32(core, measured::CORE_ID) != client_id || ObjectI32(core, measured::CORE_HOOKED_PLAYER) != -1)
            throw std::runtime_error("core/world/collision/id/hooked-player backlinks disagree");
        const auto character_position = ObjectVector(character, measured::ENTITY_POSITION);
        const auto core_position = ObjectVector(core, measured::CORE_POSITION);
        if(character_position.x != core_position.x || character_position.y != core_position.y)
            throw std::runtime_error("character entity/core positions disagree");

        const auto tuning = ReadRemote(process.Get(), tuning_pointer, measured::TUNING_SIZE);
        const auto core_tuning = core.subspan(measured::CORE_TUNING, measured::TUNING_SIZE);
        if(!std::equal(tuning.begin(), tuning.end(), core_tuning.begin()))
            throw std::runtime_error("character and effective prediction tuning disagree");
        const auto client_copy_address = AddAddress(root, measured::GAMECLIENT_CLIENT_DATA +
                                                   static_cast<std::uint64_t>(client_id) * measured::CLIENT_DATA_SIZE +
                                                   measured::CLIENT_DATA_PREDICTED_CORE);
        const auto client_copy = ReadRemote(process.Get(), client_copy_address, measured::CORE_SIZE);
        if(!CoreCopiesAgree(core, client_copy))
            throw std::runtime_error("predicted-world/client-copy physics fields disagree");

        SnapshotFrame frame;
        DecodePlayer(character, client_id, predicted_tick, frame.player);
        const auto game_bytes = ReadRemote(process.Get(), game_tiles, tile_bytes);
        const auto front_bytes = front_tiles == 0 ? std::vector<Byte>{} : ReadRemote(process.Get(), front_tiles, tile_bytes);
        frame.map = DecodeMap(width, height, game_bytes, front_bytes);

        // Double collection detects observed changes; it is not an atomic snapshot.
        const auto character_again = ReadRemote(process.Get(), character_pointer, measured::CHARACTER_SIZE);
        const auto collision_again = ReadRemote(process.Get(), collision, measured::COLLISION_SIZE);
        const auto game_again = ReadRemote(process.Get(), game_tiles, tile_bytes);
        const auto front_again = front_tiles == 0 ? std::vector<Byte>{} : ReadRemote(process.Get(), front_tiles, tile_bytes);
        const auto flags_again = ReadRemote(process.Get(), AddAddress(world, measured::WORLD_FLAGS), 24);
        if(!EqualVectors(character, character_again) || !EqualVectors(collision_bytes, collision_again) ||
           !EqualVectors(game_bytes, game_again) || !EqualVectors(front_bytes, front_again) ||
           !EqualVectors(flags, flags_again) ||
           ReadI32(process.Get(), AddAddress(client, measured::CLIENT_STATE)) != IClient::STATE_ONLINE ||
           ReadI32(process.Get(), AddAddress(client, measured::CLIENT_PREDICTED_TICKS + dummy_slot * sizeof(std::int32_t))) != predicted_tick ||
           ReadI32(process.Get(), AddAddress(world, measured::WORLD_GAME_TICK)) != predicted_tick ||
           ReadI32(process.Get(), AddAddress(world, measured::WORLD_LOCAL_CLIENT_ID)) != client_id ||
           ReadI32(process.Get(), AddAddress(snap, measured::SNAP_LOCAL_CLIENT_ID)) != client_id ||
           ReadI32(process.Get(), AddAddress(snap, measured::SNAP_PLAYER_COUNT)) != 1 ||
           ReadI32(process.Get(), local_id_address) != client_id ||
           ReadU64(process.Get(), AddAddress(root, measured::GAMECLIENT_CLIENT)) != client ||
           ReadU64(process.Get(), AddAddress(root, measured::GAMECLIENT_CONFIG)) != config ||
           !EqualVectors(tuning, ReadRemote(process.Get(), tuning_pointer, measured::TUNING_SIZE)) ||
           !EqualVectors(client_copy, ReadRemote(process.Get(), client_copy_address, measured::CORE_SIZE)))
            throw std::runtime_error("snapshot changed during bounded double-read");

        CheckIdentity();
        const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - start).count();
        if(elapsed < 0 || elapsed > 100)
            throw std::runtime_error("read-only snapshot exceeded the 100 ms collection budget");

        frame.checks = {true, true, true, true, true, true, true, true, true, true, true};
        frame.capture_note = "read-only process snapshot; double-read coherence heuristic; no input was sent";
        return frame;
    }
};

Win32Reader::Win32Reader(std::uint32_t process_id) : m_Impl(std::make_unique<Impl>(process_id)) {}
Win32Reader::~Win32Reader() = default;
Win32Reader::Win32Reader(Win32Reader &&) noexcept = default;
Win32Reader &Win32Reader::operator=(Win32Reader &&) noexcept = default;
SnapshotFrame Win32Reader::Capture() { return m_Impl->Capture(); }
} // namespace avoidfreeze::win32
