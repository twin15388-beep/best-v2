// SPDX-License-Identifier: MIT
#ifndef MALINKA_HELPER_CORE_H
#define MALINKA_HELPER_CORE_H

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace malinka::helper
{
inline constexpr std::string_view VERSION = "0.1.1-diagnostic";
inline constexpr std::size_t MAX_IMAGE_BYTES = 128U * 1024U * 1024U;
inline constexpr std::uint64_t MAX_USER_ADDRESS = 0x00007fffffffffffULL;

bool Contains(std::uint64_t size, std::uint64_t offset, std::uint64_t length);
bool UserRange(std::uint64_t address, std::uint64_t length);
// FILETIME is UTC in 100-ns units. Unknown/backwards clocks stay unavailable.
std::optional<std::uint64_t> ElapsedFileTimeMs(std::uint64_t start_100ns, std::uint64_t end_100ns);
std::string JsonString(std::string_view value);
std::string Hex(std::uint64_t value);

struct Section
{
	std::string name;
	std::uint32_t rva{}, virtual_size{}, raw_offset{}, raw_size{}, characteristics{};
};

struct Image
{
	std::uint16_t machine{};
	std::uint32_t timestamp{}, image_size{}, headers_size{};
	std::uint64_t preferred_base{}, file_size{};
	std::vector<Section> sections;
	std::optional<std::uint32_t> RawOffset(std::uint32_t rva, std::uint32_t length) const;
};

struct ImageResult
{
	std::optional<Image> image;
	std::string error;
};
ImageResult ParseImage(std::span<const std::byte> bytes);

// Measured in the PUBLIC official release, not inferred from a version string.
// Acquisition: CI43002224833/check130460886361, 2026-11-10 UTC.
// Official source tag v3.1.1 -> cf192e3fb8590dbe2e4f66a191aa6616beff6975.
// ZIP asset730984669, SHA256 51688c041453d989522658cf46ae21b08ef46b28786da204b82e46c5166d7059.
// Equality with the user's separately supplied Dropbox archive is NOT established.
struct PublicImageProfile
{
	std::string_view sha256;
	std::uint64_t file_size;
	std::uint32_t timestamp, image_size;
	std::uint32_t vtable_rva, vtable_file_offset;
	std::uint32_t locator_rva, type_descriptor_rva;
	std::array<std::uint32_t, 8> first_function_rvas;
	// No guessed game offsets. These remain unavailable, NOT zero.
	std::optional<std::uint32_t> character_state_offset;
	std::optional<std::uint32_t> collision_offset;
};
inline constexpr PublicImageProfile PUBLIC_311{
	"50430c5f1081863b956fd5f0d8e1c6b88d1aa27f79e199c4053e86a5b632902f",
	26206720, 1794305545, 50638848,
	19904440, 19898296, 24123696, 25441720,
	{1578736, 5743856, 5747008, 5819088, 5766976, 5803232, 5803168, 5772320},
	std::nullopt, std::nullopt};

bool MatchesPublicImage(const Image &image, std::string_view sha256);
bool ValidatePublicVtable(std::span<const std::byte> bytes, const Image &image);

// The documented Windows Mumble Link ABI, using explicit UTF-16 units, NOT
// host wchar_t (which is 4 bytes on Linux). No game-process structure is copied.
// Provenance: upstream src/engine/external/mumble.{h,cpp}, pinned commit above.
struct MumbleWindows
{
	std::uint32_t version, tick;
	float avatar_position[3], avatar_front[3], avatar_top[3];
	char16_t name[256];
	float camera_position[3], camera_front[3], camera_top[3];
	char16_t identity[256];
	std::uint32_t context_length;
	unsigned char context[256];
	char16_t description[2048];
};
static_assert(sizeof(float) == 4);
static_assert(sizeof(MumbleWindows) == 5460);
static_assert(offsetof(MumbleWindows, avatar_position) == 8);
static_assert(offsetof(MumbleWindows, name) == 44);
static_assert(offsetof(MumbleWindows, identity) == 592);
static_assert(offsetof(MumbleWindows, context_length) == 1104);
static_assert(offsetof(MumbleWindows, context) == 1108);

struct Telemetry
{
	std::string status{"no_mapping"};
	std::string reason{"Mumble Link has not been sampled"};
	std::optional<std::uint32_t> tick;
	std::optional<int> client_id;
	std::optional<float> x, y;
	std::optional<std::uint64_t> age_ms;
	bool observed_tick_advance{false};
	// This ABI has no writer PID or seqlock; diagnostics must NOT drive input.
};

class TelemetryTracker
{
	bool m_have_sample{false}, m_advanced{false};
	std::uint32_t m_tick{};
	std::uint64_t m_last_change{}, m_last_now{};
	int m_client_id{-1};
	std::string m_context;

public:
	void Reset();
	Telemetry Observe(const MumbleWindows &value, std::uint64_t now_ms);
	Telemetry Unavailable(std::string status, std::string reason);
};

struct FileReport
{
	std::string path, sha256;
	std::optional<Image> image;
	std::string error;
	bool recognized_public_release{false}, vtable_file_verified{false};
};
struct ScanReport
{
	std::string status{"not_run"};
	std::string reason{"An exact recognized image and matching loaded headers are required"};
	std::string started_at_utc, finished_at_utc;
	std::optional<std::uint64_t> duration_ms;
	std::uint64_t bytes_read{}, bytes_attempted{}, regions_queried{}, failed_reads{};
	std::vector<std::uint64_t> candidates;
};
struct ProcessReport
{
	std::uint32_t pid{};
	std::uint64_t creation_time_100ns{}, module_base{}, module_size{};
	std::optional<std::uint64_t> expected_creation_time_100ns;
	std::optional<std::uint64_t> age_ms_at_inspection_start, age_ms_at_scan_start;
	bool read_only_handle{false}, loaded_headers_match{false}, loaded_vtable_matches{false};
	std::string error;
	ScanReport scan;
};
struct Report
{
	FileReport file;
	std::optional<ProcessReport> process;
	Telemetry telemetry;
	bool mapping_created_by_helper{false};
	// Inspection and save are different events. None is a live physics snapshot.
	std::string measured_at_utc, inspection_finished_at_utc, saved_at_utc;
	std::optional<std::uint64_t> inspection_duration_ms;
	std::string inspection_trigger{"unspecified"};
};
std::string MakeReport(const Report &report);
} // namespace malinka::helper
#endif
