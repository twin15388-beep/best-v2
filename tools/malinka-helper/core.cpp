// SPDX-License-Identifier: MIT
#include "core.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <iomanip>
#include <limits>
#include <locale>
#include <sstream>

namespace malinka::helper
{
bool Contains(std::uint64_t size, std::uint64_t offset, std::uint64_t length)
{
	return offset <= size && length <= size - offset;
}
bool UserRange(std::uint64_t address, std::uint64_t length)
{
	return address >= 0x10000 && address <= MAX_USER_ADDRESS && length > 0 && length - 1 <= MAX_USER_ADDRESS - address;
}
std::optional<std::uint64_t> ElapsedFileTimeMs(std::uint64_t start_100ns, std::uint64_t end_100ns)
{
	if(start_100ns == 0 || end_100ns < start_100ns) return std::nullopt;
	return (end_100ns - start_100ns) / 10000;
}
std::string JsonString(std::string_view value)
{
	constexpr char DIGITS[] = "0123456789abcdef";
	std::string out{"\""};
	for(char raw : value)
	{
		const auto c = static_cast<unsigned char>(raw);
		if(c == '"' || c == '\\') { out += '\\'; out += static_cast<char>(c); }
		else if(c < 0x20) { out += "\\u00"; out += DIGITS[c >> 4]; out += DIGITS[c & 15]; }
		else out += static_cast<char>(c);
	}
	out += '"';
	return out;
}
std::string Hex(std::uint64_t value)
{
	std::ostringstream out;
	out.imbue(std::locale::classic());
	out << "0x" << std::hex << value;
	return out.str();
}

namespace
{
std::uint16_t U16(std::span<const std::byte> b, std::size_t at)
{
	return static_cast<std::uint16_t>(std::to_integer<unsigned>(b[at]) | (std::to_integer<unsigned>(b[at + 1]) << 8));
}
std::uint32_t U32(std::span<const std::byte> b, std::size_t at)
{
	return static_cast<std::uint32_t>(U16(b, at)) | (static_cast<std::uint32_t>(U16(b, at + 2)) << 16);
}
std::uint64_t U64(std::span<const std::byte> b, std::size_t at)
{
	return static_cast<std::uint64_t>(U32(b, at)) | (static_cast<std::uint64_t>(U32(b, at + 4)) << 32);
}
bool Overlap(std::uint64_t a, std::uint64_t an, std::uint64_t b, std::uint64_t bn)
{
	return an != 0 && bn != 0 && a < b + bn && b < a + an;
}
std::optional<std::string> Ascii16(std::span<const char16_t> text)
{
	std::string out;
	for(char16_t c : text)
	{
		if(c == 0) return out;
		if(c < 0x20 || c > 0x7e) return std::nullopt;
		out += static_cast<char>(c);
	}
	return std::nullopt;
}
template<typename T>
void Optional(std::ostream &out, const std::optional<T> &v)
{
	if(v && std::isfinite(static_cast<double>(*v))) out << *v; else out << "null";
}
} // namespace

std::optional<std::uint32_t> Image::RawOffset(std::uint32_t rva, std::uint32_t length) const
{
	if(length == 0 || !Contains(image_size, rva, length)) return std::nullopt;
	if(Contains(headers_size, rva, length) && Contains(file_size, rva, length)) return rva;
	for(const auto &s : sections)
	{
		if(rva >= s.rva && Contains(s.raw_size, rva - s.rva, length))
		{
			const std::uint64_t raw = static_cast<std::uint64_t>(s.raw_offset) + rva - s.rva;
			if(raw <= std::numeric_limits<std::uint32_t>::max() && Contains(file_size, raw, length))
				return static_cast<std::uint32_t>(raw);
		}
	}
	return std::nullopt;
}
ImageResult ParseImage(std::span<const std::byte> b)
{
	auto fail = [](std::string text) { return ImageResult{std::nullopt, std::move(text)}; };
	if(b.size() < 64 || b.size() > MAX_IMAGE_BYTES) return fail("Image size is outside the inspection limit");
	if(U16(b, 0) != 0x5a4d) return fail("Missing DOS MZ header");
	const std::uint32_t nt = U32(b, 60);
	if(nt < 64 || nt > 1024U * 1024U || !Contains(b.size(), nt, 24) || U32(b, nt) != 0x4550)
		return fail("Invalid PE header offset/signature");
	if(U16(b, nt + 4) != 0x8664) return fail("Only Windows AMD64 images are supported");
	const std::uint16_t count = U16(b, nt + 6), opt_size = U16(b, nt + 20);
	const std::size_t opt = static_cast<std::size_t>(nt) + 24;
	if(count == 0 || count > 96 || opt_size < 112 || opt_size > 4096 || !Contains(b.size(), opt, opt_size) || U16(b, opt) != 0x20b)
		return fail("Invalid PE32+ optional header or section count");
	const std::uint32_t dir_count = U32(b, opt + 108);
	if(dir_count > 16 || 112U + static_cast<std::uint64_t>(dir_count) * 8 > opt_size)
		return fail("Invalid data-directory count");
	Image out;
	out.machine = U16(b, nt + 4);
	out.timestamp = U32(b, nt + 8);
	out.preferred_base = U64(b, opt + 24);
	out.image_size = U32(b, opt + 56);
	out.headers_size = U32(b, opt + 60);
	out.file_size = b.size();
	const std::size_t table = opt + opt_size;
	const std::uint64_t table_size = static_cast<std::uint64_t>(count) * 40;
	if(out.image_size == 0 || out.image_size > 1024U * 1024U * 1024U || !UserRange(out.preferred_base, out.image_size) ||
		out.headers_size > b.size() || out.headers_size > out.image_size || !Contains(out.headers_size, table, table_size))
		return fail("Invalid image/header size or section-table extent");
	const std::uint32_t sa = U32(b, opt + 32), fa = U32(b, opt + 36);
	const auto pow2 = [](std::uint32_t x) { return x != 0 && (x & (x - 1)) == 0; };
	if(!pow2(sa) || !pow2(fa) || sa < fa || sa > 0x10000000 || fa > 65536 ||
		(sa < 4096 ? sa != fa : fa < 512) || out.image_size % sa != 0 || out.headers_size % fa != 0)
		return fail("Invalid PE section/file alignment");
	for(std::uint16_t n = 0; n < count; ++n)
	{
		const std::size_t at = table + static_cast<std::size_t>(n) * 40;
		Section s;
		for(std::size_t i = 0; i < 8 && b[at + i] != std::byte{}; ++i)
		{
			const unsigned char c = std::to_integer<unsigned char>(b[at + i]);
			if(c < 0x20 || c > 0x7e) return fail("Invalid section name");
			s.name += static_cast<char>(c);
		}
		s.virtual_size = U32(b, at + 8); s.rva = U32(b, at + 12);
		s.raw_size = U32(b, at + 16); s.raw_offset = U32(b, at + 20);
		s.characteristics = U32(b, at + 36);
		const std::uint64_t extent = std::max(s.virtual_size, s.raw_size);
		if(s.rva < out.headers_size || s.rva % sa != 0 || !Contains(out.image_size, s.rva, extent) ||
			(s.raw_size && (s.raw_offset < out.headers_size || s.raw_offset % fa != 0 || s.raw_size % fa != 0 || !Contains(b.size(), s.raw_offset, s.raw_size))))
			return fail("Section exceeds image/file bounds or violates alignment");
		for(const auto &prev : out.sections)
		{
			if(Overlap(s.raw_offset, s.raw_size, prev.raw_offset, prev.raw_size) ||
				Overlap(s.rva, extent, prev.rva, std::max(prev.virtual_size, prev.raw_size)))
				return fail("Overlapping PE sections");
		}
		out.sections.push_back(std::move(s));
	}
	return {std::move(out), {}};
}

bool MatchesPublicImage(const Image &image, std::string_view sha256)
{
	return sha256 == PUBLIC_311.sha256 && image.machine == 0x8664 && image.file_size == PUBLIC_311.file_size &&
		image.image_size == PUBLIC_311.image_size && image.timestamp == PUBLIC_311.timestamp;
}
bool ValidatePublicVtable(std::span<const std::byte> bytes, const Image &image)
{
	if(image.file_size != bytes.size() || PUBLIC_311.vtable_rva < 8) return false;
	constexpr char NAME[] = ".?AVCGameClient@@";
	constexpr std::uint32_t TYPE_BYTES = 16 + sizeof(NAME);
	const auto table = image.RawOffset(PUBLIC_311.vtable_rva - 8, 72);
	const auto locator = image.RawOffset(PUBLIC_311.locator_rva, 24);
	const auto type = image.RawOffset(PUBLIC_311.type_descriptor_rva, TYPE_BYTES);
	if(!table || !locator || !type || *table + 8 != PUBLIC_311.vtable_file_offset ||
		!Contains(bytes.size(), *table, 72) || !Contains(bytes.size(), *locator, 24) || !Contains(bytes.size(), *type, TYPE_BYTES)) return false;
	if(U64(bytes, *table) != image.preferred_base + PUBLIC_311.locator_rva ||
		U32(bytes, *locator) != 1 || U32(bytes, *locator + 4) != 0 || U32(bytes, *locator + 8) != 0 ||
		U32(bytes, *locator + 12) != PUBLIC_311.type_descriptor_rva || U32(bytes, *locator + 20) != PUBLIC_311.locator_rva) return false;
	if(std::memcmp(bytes.data() + *type + 16, NAME, sizeof(NAME)) != 0) return false;
	for(std::size_t i = 0; i < PUBLIC_311.first_function_rvas.size(); ++i)
	{
		const auto rva = PUBLIC_311.first_function_rvas[i];
		if(U64(bytes, *table + 8 + i * 8) != image.preferred_base + rva) return false;
		if(std::none_of(image.sections.begin(), image.sections.end(), [rva](const Section &s) {
			return rva >= s.rva && rva - s.rva < s.virtual_size && (s.characteristics & 0x20000000) != 0;
		})) return false;
	}
	return true;
}

void TelemetryTracker::Reset()
{
	m_have_sample = m_advanced = false; m_tick = 0; m_last_change = m_last_now = 0; m_client_id = -1; m_context.clear();
}
Telemetry TelemetryTracker::Unavailable(std::string status, std::string reason)
{
	Reset(); Telemetry out; out.status = std::move(status); out.reason = std::move(reason); return out;
}
Telemetry TelemetryTracker::Observe(const MumbleWindows &v, std::uint64_t now)
{
	if(v.version == 0) return Unavailable("no_writer", "The shared section exists but no compatible writer has initialized it");
	if(v.version != 2) return Unavailable("invalid", "Unsupported Mumble Link version");
	const auto name = Ascii16(v.name), id = Ascii16(v.identity);
	if(!name || *name != "DDNet" || !id || v.context_length >= sizeof(v.context))
		return Unavailable("invalid", "Unexpected name, unterminated identity or invalid context length");
	if(v.context_length == 0 || id->empty()) return Unavailable("offline", "The writer has no online player identity/context");
	if(id->size() > 3 || !std::all_of(id->begin(), id->end(), [](char c) { return c >= '0' && c <= '9'; }))
		return Unavailable("invalid", "Player identity is not an ASCII client ID");
	int client_id = 0;
	for(char c : *id) client_id = client_id * 10 + c - '0';
	if(client_id >= MAX_TELEMETRY_CLIENTS || (id->size() > 1 && id->front() == '0')) return Unavailable("invalid", "Client ID is outside the documented range/encoding");
	if(v.context[v.context_length] != 0) return Unavailable("invalid", "Context is not terminated at its declared length");
	std::string context;
	for(std::uint32_t n = 0; n < v.context_length; ++n)
	{
		if(v.context[n] < 0x20 || v.context[n] > 0x7e) return Unavailable("invalid", "Unexpected context encoding");
		context += static_cast<char>(v.context[n]);
	}
	for(float p : v.avatar_position)
	{
		if(!std::isfinite(p) || std::abs(static_cast<double>(p)) * 128.0 > 1.0e9)
			return Unavailable("invalid", "Non-finite or implausible rendered position");
	}
	if(v.avatar_position[1] != 0.0f) return Unavailable("invalid", "Not the documented DDNet 2D coordinate convention");
	const bool session_changed = !m_have_sample || m_client_id != client_id || m_context != context || now < m_last_now;
	const std::uint32_t delta = v.tick - m_tick; // unsigned wraparound is intentional
	if(session_changed || delta >= 0x80000000U)
	{
		m_advanced = false; m_last_change = now;
	}
	else if(delta != 0)
	{
		m_advanced = true; m_last_change = now;
	}
	m_have_sample = true; m_tick = v.tick; m_client_id = client_id; m_context = std::move(context); m_last_now = now;
	Telemetry out;
	out.tick = v.tick; out.client_id = client_id; out.age_ms = now - m_last_change; out.observed_tick_advance = m_advanced;
	if(*out.age_ms > 250)
	{
		out.status = "stale"; out.reason = "No tick advance for more than 250 ms; not a usable live state";
		return out;
	}
	out.x = v.avatar_position[0] * 128.0f; out.y = v.avatar_position[2] * 128.0f;
	out.status = m_advanced ? "live_diagnostic" : "warming";
	out.reason = "Rendered position only; writer PID, physics tick, velocity, collision and input are NOT provided";
	return out;
}

std::string MakeReport(const Report &r)
{
	std::ostringstream o;
	o.imbue(std::locale::classic()); o << std::setprecision(9);
	o << "{\n  \"schema\": 2,\n  \"tool\": \"MalinkaHelper\",\n  \"version\": " << JsonString(VERSION)
	  << ",\n  \"measured_at_utc\": " << (r.measured_at_utc.empty() ? "null" : JsonString(r.measured_at_utc))
	  << ",\n  \"inspection_finished_at_utc\": " << (r.inspection_finished_at_utc.empty() ? "null" : JsonString(r.inspection_finished_at_utc))
	  << ",\n  \"saved_at_utc\": " << (r.saved_at_utc.empty() ? "null" : JsonString(r.saved_at_utc))
	  << ",\n  \"inspection_trigger\": " << JsonString(r.inspection_trigger)
	  << ",\n  \"inspection_duration_ms\": "; Optional(o, r.inspection_duration_ms);
	o << ",\n  \"automatic_avoid\": {\"enabled\": false, \"status\": \"BLOCKED\", \"reason\": \"No validated character/collision/tuning/input adapter; diagnostics are not Avoid Freeze\"},"
	  << "\n  \"policy\": {\"writes_client_files\": false, \"writes_process_memory\": false, \"injects_dll\": false, \"sends_input\": false, \"official_badge_verified\": false},"
	  << "\n  \"file\": {\"path\": " << JsonString(r.file.path) << ", \"sha256\": " << JsonString(r.file.sha256)
	  << ", \"error\": " << JsonString(r.file.error) << ", \"recognized_public_release\": " << (r.file.recognized_public_release ? "true" : "false")
	  << ", \"vtable_file_verified\": " << (r.file.vtable_file_verified ? "true" : "false");
	if(r.file.image)
	{
		const auto &i = *r.file.image;
		o << ", \"machine\": " << i.machine << ", \"bytes\": " << i.file_size << ", \"timestamp\": " << i.timestamp
		  << ", \"size_of_image\": " << i.image_size << ", \"preferred_image_base\": " << JsonString(Hex(i.preferred_base));
	}
	o << "},\n  \"process\": ";
	if(!r.process) o << "null";
	else
	{
		const auto &p = *r.process;
		o << "{\"pid\": " << p.pid << ", \"measurement_only_not_a_live_snapshot\": true, \"creation_time_100ns\": " << (p.creation_time_100ns ? std::to_string(p.creation_time_100ns) : "null")
		  << ", \"expected_creation_time_100ns\": "; Optional(o, p.expected_creation_time_100ns);
		o << ", \"age_ms_at_inspection_start\": "; Optional(o, p.age_ms_at_inspection_start);
		o << ", \"age_ms_at_scan_start\": "; Optional(o, p.age_ms_at_scan_start);
		o << ", \"module_base_runtime_va\": " << (UserRange(p.module_base, p.module_size) ? JsonString(Hex(p.module_base)) : "null") << ", \"module_size\": " << (p.module_size ? std::to_string(p.module_size) : "null")
		  << ", \"handle_read_only\": " << (p.read_only_handle ? "true" : "false")
		  << ", \"loaded_headers_match\": " << (p.loaded_headers_match ? "true" : "false")
		  << ", \"loaded_vtable_matches\": " << (p.loaded_vtable_matches ? "true" : "false") << ", \"error\": " << JsonString(p.error)
		  << ", \"root_scan\": {\"status\": " << JsonString(p.scan.status) << ", \"reason\": " << JsonString(p.scan.reason)
		  << ", \"scope\": \"Committed readable MEM_PRIVATE, 8-byte-aligned pointer matches; not an atomic snapshot\""
		  << ", \"started_at_utc\": " << (p.scan.started_at_utc.empty() ? "null" : JsonString(p.scan.started_at_utc))
		  << ", \"finished_at_utc\": " << (p.scan.finished_at_utc.empty() ? "null" : JsonString(p.scan.finished_at_utc))
		  << ", \"duration_ms\": "; Optional(o, p.scan.duration_ms);
		o << ", \"bytes_read\": " << p.scan.bytes_read << ", \"bytes_attempted\": " << p.scan.bytes_attempted
		  << ", \"regions_queried\": " << p.scan.regions_queried << ", \"failed_reads\": " << p.scan.failed_reads << ", \"candidate_runtime_vas\": [";
		for(std::size_t n = 0; n < p.scan.candidates.size(); ++n) { if(n) o << ','; o << JsonString(Hex(p.scan.candidates[n])); }
		o << "], \"candidates_are_validated_game_state\": false}}";
	}
	o << ",\n  \"layout\": {\"source_commit\": \"cf192e3fb8590dbe2e4f66a191aa6616beff6975\", \"profile_scope\": \"Exact public release SHA256 only; not every v3.1.1 build\", \"gameclient_vtable_rva\": ";
	if(r.file.recognized_public_release && r.file.vtable_file_verified) o << PUBLIC_311.vtable_rva; else o << "null";
	o << ", \"gameclient_vtable_file_offset\": ";
	if(r.file.recognized_public_release && r.file.vtable_file_verified) o << PUBLIC_311.vtable_file_offset; else o << "null";
	o << ", \"character_state_offset\": null, \"collision_offset\": null, \"physics_tuning\": null, \"verified_input_bindings\": null},"
	  << "\n  \"mumble\": {\"kind\": \"public_render_telemetry_only\", \"mapping_created_by_helper\": " << (r.mapping_created_by_helper ? "true" : "false")
	  << ", \"status\": " << JsonString(r.telemetry.status) << ", \"reason\": " << JsonString(r.telemetry.reason)
	  << ", \"writer_pid\": null, \"physics_snapshot_validated\": false, \"tick_is_physics_tick\": false, \"abi_is_seqlock\": false, \"tick\": ";
	Optional(o, r.telemetry.tick); o << ", \"client_id\": "; Optional(o, r.telemetry.client_id);
	o << ", \"render_x\": "; Optional(o, r.telemetry.x); o << ", \"render_y\": "; Optional(o, r.telemetry.y);
	o << ", \"age_ms\": "; Optional(o, r.telemetry.age_ms); o << "}\n}\n";
	return o.str();
}
} // namespace malinka::helper
