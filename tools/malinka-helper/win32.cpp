// SPDX-License-Identifier: MIT
#include "win32.h"

#include <bcrypt.h>
#include <tlhelp32.h>

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <limits>
#include <memory>
#include <stdexcept>

namespace malinka::helper
{
std::string Utf8(std::wstring_view value)
{
	if(value.empty()) return {};
	if(value.size() > 32768) throw std::runtime_error("UTF-16 text exceeds conversion limit");
	const int n = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()), nullptr, 0, nullptr, nullptr);
	if(n <= 0) throw std::runtime_error(WinError("WideCharToMultiByte"));
	std::string out(static_cast<std::size_t>(n), '\0');
	if(WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()), out.data(), n, nullptr, nullptr) != n)
		throw std::runtime_error(WinError("WideCharToMultiByte"));
	return out;
}
std::wstring Wide(std::string_view value)
{
	if(value.empty()) return {};
	if(value.size() > 128U * 1024U) return L"Text exceeds display limit";
	const int n = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()), nullptr, 0);
	if(n <= 0) return L"Invalid UTF-8 text";
	std::wstring out(static_cast<std::size_t>(n), L'\0');
	if(MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()), out.data(), n) != n)
		return L"UTF-8 conversion failed";
	return out;
}
std::string WinError(std::string_view operation, DWORD error)
{
	return std::string(operation) + " failed (Win32 " + std::to_string(error) + ")";
}
std::string UtcNow()
{
	SYSTEMTIME t{}; GetSystemTime(&t);
	char text[40]{};
	const int n = std::snprintf(text, sizeof(text), "%04u-%02u-%02uT%02u:%02u:%02u.%03uZ",
		static_cast<unsigned>(t.wYear), static_cast<unsigned>(t.wMonth), static_cast<unsigned>(t.wDay),
		static_cast<unsigned>(t.wHour), static_cast<unsigned>(t.wMinute), static_cast<unsigned>(t.wSecond), static_cast<unsigned>(t.wMilliseconds));
	return n > 0 && static_cast<std::size_t>(n) < sizeof(text) ? text : "unavailable";
}
std::uint64_t MonotonicMs()
{
	return static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count());
}
namespace
{
std::uint64_t FileTimeNow100ns()
{
	FILETIME now{}; GetSystemTimePreciseAsFileTime(&now);
	return (static_cast<std::uint64_t>(now.dwHighDateTime) << 32) | now.dwLowDateTime;
}
std::optional<std::wstring> FinalPath(HANDLE handle)
{
	std::vector<wchar_t> buf(32768);
	const DWORD n = GetFinalPathNameByHandleW(handle, buf.data(), static_cast<DWORD>(buf.size()), FILE_NAME_NORMALIZED | VOLUME_NAME_DOS);
	if(n == 0 || n >= buf.size()) return std::nullopt;
	return std::wstring(buf.data(), n);
}
bool EqualPath(std::wstring_view a, std::wstring_view b)
{
	if(a.size() > 32768 || b.size() > 32768) return false;
	return CompareStringOrdinal(a.data(), static_cast<int>(a.size()), b.data(), static_cast<int>(b.size()), TRUE) == CSTR_EQUAL;
}
bool InsideDirectory(std::wstring_view file, std::wstring directory)
{
	while(!directory.empty() && (directory.back() == L'\\' || directory.back() == L'/')) directory.pop_back();
	return file.size() > directory.size() && EqualPath(file.substr(0, directory.size()), directory) && (file[directory.size()] == L'\\' || file[directory.size()] == L'/');
}
std::optional<std::wstring> ResolveFile(const std::wstring &path)
{
	Handle f(CreateFileW(path.c_str(), FILE_READ_ATTRIBUTES, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr));
	if(!f) return std::nullopt;
	return FinalPath(f.Get());
}
std::string Sha256(std::span<const std::byte> data)
{
	BCRYPT_ALG_HANDLE algorithm = nullptr;
	if(BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0) < 0) throw std::runtime_error("BCryptOpenAlgorithmProvider failed");
	struct AlgorithmGuard { BCRYPT_ALG_HANDLE h; ~AlgorithmGuard() { BCryptCloseAlgorithmProvider(h, 0); } } ag{algorithm};
	DWORD object_size{}, hash_size{}, used{};
	if(BCryptGetProperty(algorithm, BCRYPT_OBJECT_LENGTH, reinterpret_cast<PUCHAR>(&object_size), sizeof(object_size), &used, 0) < 0 || used != sizeof(object_size) ||
		BCryptGetProperty(algorithm, BCRYPT_HASH_LENGTH, reinterpret_cast<PUCHAR>(&hash_size), sizeof(hash_size), &used, 0) < 0 || used != sizeof(hash_size) ||
		object_size == 0 || object_size > 1024U * 1024U || hash_size != 32) throw std::runtime_error("Invalid BCrypt SHA-256 properties");
	std::vector<UCHAR> object(object_size);
	BCRYPT_HASH_HANDLE hash = nullptr;
	if(BCryptCreateHash(algorithm, &hash, object.data(), object_size, nullptr, 0, 0) < 0) throw std::runtime_error("BCryptCreateHash failed");
	struct HashGuard { BCRYPT_HASH_HANDLE h; ~HashGuard() { BCryptDestroyHash(h); } } hg{hash};
	std::array<UCHAR, 32> digest{};
	if(data.size() > std::numeric_limits<ULONG>::max() || BCryptHashData(hash, reinterpret_cast<PUCHAR>(const_cast<std::byte *>(data.data())), static_cast<ULONG>(data.size()), 0) < 0 ||
		BCryptFinishHash(hash, digest.data(), static_cast<ULONG>(digest.size()), 0) < 0) throw std::runtime_error("BCrypt SHA-256 failed");
	constexpr char HEX[] = "0123456789abcdef";
	std::string out;
	for(UCHAR c : digest) { out += HEX[c >> 4]; out += HEX[c & 15]; }
	return out;
}
bool ModuleRead(HANDLE process, std::uint64_t base, std::uint64_t size, std::uint64_t offset, std::span<std::byte> bytes)
{
	return UserRange(base, size) && Contains(size, offset, bytes.size()) && ReadExact(process, base + offset, bytes);
}
bool MatchLoadedHeaders(HANDLE process, std::uint64_t base, std::uint64_t size, const Image &disk)
{
	IMAGE_DOS_HEADER dos{};
	if(!ModuleRead(process, base, size, 0, std::as_writable_bytes(std::span{&dos, 1})) || dos.e_magic != IMAGE_DOS_SIGNATURE || dos.e_lfanew < static_cast<LONG>(sizeof(dos)) || dos.e_lfanew > 1024 * 1024) return false;
	IMAGE_NT_HEADERS64 nt{};
	if(!ModuleRead(process, base, size, static_cast<std::uint32_t>(dos.e_lfanew), std::as_writable_bytes(std::span{&nt, 1})) ||
		nt.Signature != IMAGE_NT_SIGNATURE || nt.FileHeader.Machine != IMAGE_FILE_MACHINE_AMD64 || nt.OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR64_MAGIC ||
		nt.FileHeader.SizeOfOptionalHeader < sizeof(IMAGE_OPTIONAL_HEADER64)) return false;
	return nt.FileHeader.TimeDateStamp == disk.timestamp && nt.OptionalHeader.SizeOfImage == disk.image_size &&
		size == disk.image_size && nt.OptionalHeader.SizeOfHeaders == disk.headers_size;
}
bool MatchLoadedVtable(HANDLE process, std::uint64_t base, std::uint64_t size)
{
	std::array<std::uint64_t, 9> pointers{};
	if(!ModuleRead(process, base, size, PUBLIC_311.vtable_rva - 8, std::as_writable_bytes(std::span{pointers}))) return false;
	if(pointers[0] != base + PUBLIC_311.locator_rva) return false;
	for(std::size_t i = 0; i < PUBLIC_311.first_function_rvas.size(); ++i)
		if(pointers[i + 1] != base + PUBLIC_311.first_function_rvas[i]) return false;
	return true;
}
} // namespace

FileReport InspectFile(const std::wstring &path)
{
	FileReport out;
	try
	{
		out.path = Utf8(path);
		// No write sharing: a concurrent writer must not change bytes during hashing.
		Handle f(CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, nullptr));
		if(!f) { out.error = WinError("Open image read-only"); return out; }
		BY_HANDLE_FILE_INFORMATION before{}, after{};
		if(GetFileType(f.Get()) != FILE_TYPE_DISK || !GetFileInformationByHandle(f.Get(), &before) || (before.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
		{ out.error = "Not a readable regular disk file"; return out; }
		const auto full_path = FinalPath(f.Get());
		if(!full_path) { out.error = WinError("Resolve image path"); return out; }
		out.path = Utf8(*full_path);
		const std::uint64_t size = (static_cast<std::uint64_t>(before.nFileSizeHigh) << 32) | before.nFileSizeLow;
		if(size < 64 || size > MAX_IMAGE_BYTES) { out.error = "Image file is outside the 64-byte..128-MiB inspection limit"; return out; }
		std::vector<std::byte> bytes(static_cast<std::size_t>(size));
		std::size_t offset = 0;
		while(offset < bytes.size())
		{
			const auto requested = static_cast<DWORD>(std::min<std::size_t>(65536, bytes.size() - offset));
			DWORD read{};
			if(!ReadFile(f.Get(), bytes.data() + offset, requested, &read, nullptr) || read != requested)
			{ out.error = "Image file read failed or was partial"; return out; }
			offset += read;
		}
		if(!GetFileInformationByHandle(f.Get(), &after) || before.nFileSizeHigh != after.nFileSizeHigh || before.nFileSizeLow != after.nFileSizeLow ||
			before.nFileIndexHigh != after.nFileIndexHigh || before.nFileIndexLow != after.nFileIndexLow ||
			CompareFileTime(&before.ftLastWriteTime, &after.ftLastWriteTime) != 0)
		{ out.error = "Image changed during inspection"; return out; }
		out.sha256 = Sha256(bytes);
		auto parsed = ParseImage(bytes);
		out.image = std::move(parsed.image); out.error = std::move(parsed.error);
		if(out.image)
		{
			out.recognized_public_release = MatchesPublicImage(*out.image, out.sha256);
			out.vtable_file_verified = out.recognized_public_release && ValidatePublicVtable(bytes, *out.image);
			if(out.recognized_public_release && !out.vtable_file_verified) out.error = "Recognized hash but measured RTTI/vtable failed revalidation";
		}
	}
	catch(const std::exception &e) { out.error = e.what(); }
	return out;
}

bool ReadExact(HANDLE process, std::uint64_t address, std::span<std::byte> output)
{
	if(!process || output.empty() || output.size() > 1024U * 1024U || !UserRange(address, output.size())) return false;
	SIZE_T read{};
	return ReadProcessMemory(process, reinterpret_cast<LPCVOID>(static_cast<std::uintptr_t>(address)), output.data(), output.size(), &read) != FALSE && read == output.size();
}
static ScanReport ScanPrivatePointersImpl(HANDLE process, std::uint64_t pointer, std::stop_token stop)
{
	ScanReport out;
	if(!UserRange(pointer, 8)) { out.status = "blocked"; out.reason = "Invalid validated vtable pointer"; return out; }
	out.status = "incomplete";
	out.reason = "Bounded read-only search; candidates are not validated character state";
	constexpr std::uint64_t MAX_BYTES = 512ULL * 1024ULL * 1024ULL;
	constexpr std::uint64_t MAX_REGIONS = 65536;
	const auto start = MonotonicMs();
	std::array<std::byte, 65536> buffer{};
	std::uint64_t address = 0x10000;
	while(address <= MAX_USER_ADDRESS)
	{
		if(stop.stop_requested()) { out.status = "cancelled"; out.reason = "Inspection cancelled"; return out; }
		if(WaitForSingleObject(process, 0) != WAIT_TIMEOUT) { out.status = "exited"; out.reason = "Target exited or liveness check failed"; out.candidates.clear(); return out; }
		if(out.regions_queried >= MAX_REGIONS || out.bytes_attempted >= MAX_BYTES || MonotonicMs() - start >= 1500)
		{ out.reason = "Search budget reached; no uniqueness claim is possible"; return out; }
		MEMORY_BASIC_INFORMATION info{};
		if(VirtualQueryEx(process, reinterpret_cast<LPCVOID>(static_cast<std::uintptr_t>(address)), &info, sizeof(info)) != sizeof(info))
		{
			if(GetLastError() == ERROR_INVALID_PARAMETER) { out.status = out.failed_reads ? "incomplete" : "complete"; out.reason = "End of user address space; pointer matches remain unvalidated candidates"; }
			else out.reason = WinError("VirtualQueryEx");
			return out;
		}
		++out.regions_queried;
		const auto region_base = reinterpret_cast<std::uintptr_t>(info.BaseAddress);
		const auto region_size = static_cast<std::uint64_t>(info.RegionSize);
		if(region_base > address || region_size == 0 || !UserRange(region_base, region_size) || region_base + region_size <= address)
		{ out.reason = "Invalid/inconsistent virtual-memory region"; return out; }
		const auto protection = info.Protect & 0xff;
		const bool readable = protection == PAGE_READONLY || protection == PAGE_READWRITE || protection == PAGE_WRITECOPY ||
			protection == PAGE_EXECUTE_READ || protection == PAGE_EXECUTE_READWRITE || protection == PAGE_EXECUTE_WRITECOPY;
		if(info.State == MEM_COMMIT && info.Type == MEM_PRIVATE && readable && (info.Protect & (PAGE_GUARD | PAGE_NOACCESS)) == 0)
		{
			std::uint64_t offset = address - region_base;
			for(; offset < region_size;)
			{
				if(stop.stop_requested()) { out.status = "cancelled"; out.reason = "Inspection cancelled"; return out; }
				if(out.bytes_attempted >= MAX_BYTES || MonotonicMs() - start >= 1500)
				{ out.reason = "Search byte/time budget reached; candidates cannot be considered unique"; return out; }
				const auto length = static_cast<std::size_t>(std::min<std::uint64_t>({region_size - offset, buffer.size(), MAX_BYTES - out.bytes_attempted}));
				if(length == 0) return out;
				out.bytes_attempted += length;
				if(ReadExact(process, region_base + offset, std::span{buffer}.first(length)))
				{
					out.bytes_read += length;
					for(std::size_t i = 0; i + sizeof(pointer) <= length; i += alignof(std::uint64_t))
					{
						std::uint64_t value{}; std::memcpy(&value, buffer.data() + i, sizeof(value));
						if(value == pointer)
						{
							out.candidates.push_back(region_base + offset + i);
							if(out.candidates.size() >= 32) { out.reason = "Candidate-count limit reached; ambiguous search"; return out; }
						}
					}
				}
				else ++out.failed_reads; // Never scan the valid prefix of a partial read.
				offset += length;
			}
		}
		address = region_base + region_size;
	}
	out.status = out.failed_reads ? "incomplete" : "complete";
	out.reason = "End of user address space; no object/physics validation has been performed";
	return out;
}

ScanReport ScanPrivatePointers(HANDLE process, std::uint64_t pointer, std::stop_token stop)
{
	const auto start = MonotonicMs(); const auto started_at = UtcNow();
	auto out = ScanPrivatePointersImpl(process, pointer, stop);
	out.started_at_utc = started_at; out.finished_at_utc = UtcNow(); out.duration_ms = MonotonicMs() - start;
	if(out.status == "complete" && out.candidates.empty())
		out.reason = "No pointer matches during this private-memory enumeration; not an atomic snapshot. Initialization may still be in progress: recheck after the client is ready";
	return out;
}

static Report InspectProcessImpl(DWORD pid, std::stop_token stop, std::optional<std::uint64_t> expected_creation_time_100ns)
{
	const auto started_100ns = FileTimeNow100ns();
	Report out; out.measured_at_utc = UtcNow(); out.process.emplace(); auto &p = *out.process; p.pid = pid;
	p.expected_creation_time_100ns = expected_creation_time_100ns;
	try
	{
		// Deliberately NO VM_WRITE/VM_OPERATION/CREATE_THREAD/ALL_ACCESS/debug privilege.
		constexpr DWORD RIGHTS = PROCESS_QUERY_LIMITED_INFORMATION | PROCESS_VM_READ | SYNCHRONIZE;
		Handle process(OpenProcess(RIGHTS, FALSE, pid));
		if(!process) { p.error = WinError("OpenProcess read-only"); return out; }
		p.read_only_handle = true;
		if(WaitForSingleObject(process.Get(), 0) != WAIT_TIMEOUT) { p.error = "Target is not running"; return out; }
		USHORT process_machine{}, native_machine{};
		if(!IsWow64Process2(process.Get(), &process_machine, &native_machine) || process_machine != IMAGE_FILE_MACHINE_UNKNOWN || native_machine != IMAGE_FILE_MACHINE_AMD64)
		{ p.error = "Target is not a native AMD64 process"; return out; }
		FILETIME creation{}, exit{}, kernel{}, user{};
		if(!GetProcessTimes(process.Get(), &creation, &exit, &kernel, &user)) { p.error = WinError("GetProcessTimes"); return out; }
		p.creation_time_100ns = (static_cast<std::uint64_t>(creation.dwHighDateTime) << 32) | creation.dwLowDateTime;
		p.age_ms_at_inspection_start = ElapsedFileTimeMs(p.creation_time_100ns, started_100ns);
		if(expected_creation_time_100ns && (*expected_creation_time_100ns == 0 || *expected_creation_time_100ns != p.creation_time_100ns))
		{ p.error = "Process identity changed (PID reuse); attach again explicitly"; p.scan.status = "blocked_process_identity_changed"; return out; }
		if(stop.stop_requested()) { p.error = "Inspection cancelled"; p.scan.status = "cancelled"; return out; }
		std::array<wchar_t, 32768> path{}; DWORD length = static_cast<DWORD>(path.size());
		if(!QueryFullProcessImageNameW(process.Get(), 0, path.data(), &length) || length == 0 || length >= path.size())
		{ p.error = WinError("QueryFullProcessImageNameW"); return out; }
		out.file = InspectFile(std::wstring(path.data(), length));
		if(!out.file.image || !out.file.error.empty()) { p.error = "Cannot validate on-disk process image"; return out; }
		Handle modules;
		for(unsigned n = 0; n < 4; ++n)
		{
			modules.Reset(CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid));
			if(modules || GetLastError() != ERROR_BAD_LENGTH) break;
			if(stop.stop_requested()) { p.error = "Inspection cancelled"; return out; }
		}
		if(!modules) { p.error = WinError("Snapshot modules"); return out; }
		MODULEENTRY32W entry{}; entry.dwSize = sizeof(entry);
		if(!Module32FirstW(modules.Get(), &entry)) { p.error = WinError("Module32FirstW"); return out; }
		bool found = false;
		for(unsigned n = 0; n < 1024; ++n)
		{
			const auto module_path = ResolveFile(entry.szExePath);
			if(module_path && EqualPath(*module_path, Wide(out.file.path)))
			{
				p.module_base = reinterpret_cast<std::uintptr_t>(entry.modBaseAddr); p.module_size = entry.modBaseSize;
				found = true; break;
			}
			if(!Module32NextW(modules.Get(), &entry)) break;
		}
		if(!found || !UserRange(p.module_base, p.module_size)) { p.error = "Main executable module could not be safely identified"; return out; }
		p.loaded_headers_match = MatchLoadedHeaders(process.Get(), p.module_base, p.module_size, *out.file.image);
		if(!p.loaded_headers_match) { p.error = "Loaded PE headers differ from the inspected file"; return out; }
		if(!out.file.recognized_public_release || !out.file.vtable_file_verified)
		{ p.scan.status = "blocked_unknown_build"; p.scan.reason = "SHA-256 is not the specifically audited official image; no game addresses will be read"; return out; }
		p.loaded_vtable_matches = MatchLoadedVtable(process.Get(), p.module_base, p.module_size);
		if(!p.loaded_vtable_matches) { p.error = "Loaded vtable does not match the measured, ASLR-adjusted profile"; return out; }
		p.age_ms_at_scan_start = ElapsedFileTimeMs(p.creation_time_100ns, FileTimeNow100ns());
		p.scan = ScanPrivatePointers(process.Get(), p.module_base + PUBLIC_311.vtable_rva, stop);
		if(WaitForSingleObject(process.Get(), 0) != WAIT_TIMEOUT)
		{
			p.scan.candidates.clear(); p.scan.status = "exited";
			p.scan.reason = "Target exited or liveness check failed before inspection finished"; p.error = p.scan.reason;
		}
	}
	catch(const std::exception &e) { p.error = e.what(); }
	return out;
}
Report InspectProcess(DWORD pid, std::stop_token stop, std::optional<std::uint64_t> expected_creation_time_100ns)
{
	const auto start = MonotonicMs();
	auto out = InspectProcessImpl(pid, stop, expected_creation_time_100ns);
	out.inspection_finished_at_utc = UtcNow(); out.inspection_duration_ms = MonotonicMs() - start;
	return out;
}
Report InspectFileReport(const std::wstring &path)
{
	const auto start = MonotonicMs();
	Report out; out.measured_at_utc = UtcNow(); out.file = InspectFile(path);
	out.inspection_finished_at_utc = UtcNow(); out.inspection_duration_ms = MonotonicMs() - start;
	return out;
}
std::optional<DWORD> LaunchClient(const std::wstring &path, std::string &error)
{
	const auto inspected = InspectFile(path);
	if(!inspected.image || !inspected.error.empty()) { error = inspected.error; return std::nullopt; }
	const auto full = Wide(inspected.path);
	if(full.find(L'"') != std::wstring::npos) { error = "Invalid executable path"; return std::nullopt; }
	std::wstring command = L"\"" + full + L"\"";
	STARTUPINFOW startup{}; startup.cb = sizeof(startup);
	PROCESS_INFORMATION process{};
	const auto directory = std::filesystem::path(full).parent_path().wstring();
	if(!CreateProcessW(full.c_str(), command.data(), nullptr, nullptr, FALSE, 0, nullptr, directory.c_str(), &startup, &process))
	{ error = WinError("CreateProcessW"); return std::nullopt; }
	// CreateProcess initially returns broad handles; close them immediately.
	// Any later inspection reopens the PID with the explicit read-only mask.
	Handle thread(process.hThread), initial_process(process.hProcess);
	return process.dwProcessId;
}

MumbleReader::~MumbleReader()
{
	if(m_view) UnmapViewOfFile(m_view);
}
bool MumbleReader::Open(std::string &error)
{
	if(m_view) return true;
	m_mapping.Reset(OpenFileMappingW(FILE_MAP_READ, FALSE, L"MumbleLink"));
	if(!m_mapping)
	{
		if(GetLastError() != ERROR_FILE_NOT_FOUND) { error = WinError("Open MumbleLink read-only"); return false; }
		// Standard consumer-owned section, initially zeroed by Windows. The
		// CLIENT needs write access; the helper's view remains FILE_MAP_READ.
		m_mapping.Reset(CreateFileMappingW(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0, sizeof(MumbleWindows), L"MumbleLink"));
		const DWORD last = GetLastError();
		if(!m_mapping) { error = WinError("Create MumbleLink section", last); return false; }
		m_created = last != ERROR_ALREADY_EXISTS;
	}
	m_view = MapViewOfFile(m_mapping.Get(), FILE_MAP_READ, 0, 0, sizeof(MumbleWindows));
	if(!m_view) { error = WinError("Map MumbleLink read-only"); m_mapping.Reset(); return false; }
	return true;
}
Telemetry MumbleReader::Sample()
{
	if(!m_view) return m_tracker.Unavailable("no_mapping", "The public shared section is unavailable");
	MumbleWindows a{}, b{};
	// This is a diagnostic best-effort double collect, NOT a seqlock. A writer
	// paused mid-update cannot be excluded, so no automatic input consumes it.
	for(unsigned n = 0; n < 3; ++n)
	{
		std::memcpy(&a, m_view, sizeof(a)); MemoryBarrier();
		std::memcpy(&b, m_view, sizeof(b)); MemoryBarrier();
		if(std::memcmp(&a, &b, sizeof(a)) == 0) return m_tracker.Observe(b, MonotonicMs());
	}
	return m_tracker.Unavailable("incoherent", "Shared telemetry changed during all three bounded reads");
}

bool WriteNewReport(const std::wstring &path, const Report &report, std::string &error)
{
	try
	{
		const std::filesystem::path requested(path);
		if(!requested.is_absolute() || requested.filename().empty()) { error = "Choose an absolute report filename"; return false; }
		std::optional<std::wstring> protected_directory;
		if(!report.file.path.empty())
		{
			const auto client = ResolveFile(Wide(report.file.path));
			if(!client) { error = "Cannot resolve the inspected client path; refusing report output"; return false; }
			protected_directory = std::filesystem::path(*client).parent_path().wstring();
		}
		Handle parent(CreateFileW(requested.parent_path().c_str(), FILE_READ_ATTRIBUTES, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, nullptr));
		const auto resolved_parent = parent ? FinalPath(parent.Get()) : std::nullopt;
		if(!resolved_parent) { error = "Cannot resolve report directory"; return false; }
		if(protected_directory && (EqualPath(*resolved_parent, *protected_directory) || InsideDirectory(*resolved_parent, *protected_directory)))
		{ error = "Report output inside the inspected client's directory is forbidden; choose a different folder"; return false; }
		Report saved = report; saved.saved_at_utc = UtcNow();
		const auto text = MakeReport(saved);
		if(text.size() > 128U * 1024U) { error = "Report exceeded the output limit"; return false; }
		Handle file(CreateFileW(path.c_str(), GENERIC_WRITE | DELETE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr));
		if(!file) { error = WinError("Create NEW report (existing files are never replaced)"); return false; }
		bool keep = false;
		struct Rollback { HANDLE h; bool &keep; ~Rollback() { if(!keep) { FILE_DISPOSITION_INFO d{TRUE}; SetFileInformationByHandle(h, FileDispositionInfo, &d, sizeof(d)); } } } rollback{file.Get(), keep};
		const auto actual = FinalPath(file.Get());
		if(!actual || (protected_directory && InsideDirectory(*actual, *protected_directory)))
		{ error = "Report destination changed or resolved inside the client's directory"; return false; }
		DWORD written{};
		if(!WriteFile(file.Get(), text.data(), static_cast<DWORD>(text.size()), &written, nullptr) || written != text.size() || !FlushFileBuffers(file.Get()))
		{ error = "Report write/flush failed; incomplete owned report is being removed"; return false; }
		keep = true; return true;
	}
	catch(const std::exception &e) { error = e.what(); return false; }
}
} // namespace malinka::helper
